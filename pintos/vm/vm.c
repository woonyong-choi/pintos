/* vm.c: Generic interface for virtual memory objects. */

#include <string.h>
#include "threads/malloc.h"
#include "threads/vaddr.h"
#include "vm/vm.h"
#include "vm/inspect.h"
#include "hash.h"
#include "userprog/process.h"
#include "threads/mmu.h"

static struct list frame_table;

/* 각 하위 시스템의 초기화 코드를 호출해 가상 메모리 하위 시스템을 초기화한다. */
void
vm_init (void) {
	vm_anon_init ();
	vm_file_init ();
#ifdef EFILESYS  /* For project 4 */
	pagecache_init ();
#endif
	register_inspect_intr ();
	/* DO NOT MODIFY UPPER LINES. */
	/* TODO: Your code goes here. */
	list_init(&frame_table);
}

/* 페이지 타입을 얻는다. 아직 초기화되지 않은 페이지가 실제로 어떤 타입으로
 * 초기화될 예정인지 알고 싶을 때 유용하다.
 * 이 함수는 현재 완성되어 있다. */
enum vm_type
page_get_type (struct page *page) {
	int ty = VM_TYPE (page->operations->type);
	switch (ty) {
		case VM_UNINIT:
			return VM_TYPE (page->uninit.type);
		default:
			return ty;
	}
}

/* 보조 함수들 */
static struct frame *vm_get_victim (void);
static bool vm_do_claim_page (struct page *page);
static struct frame *vm_evict_frame (void);
static uint64_t page_hash (const struct hash_elem *e, void *aux UNUSED);
static bool page_less (const struct hash_elem *a, const struct hash_elem *b, void *aux UNUSED);

static uint64_t
page_hash (const struct hash_elem *e, void *aux UNUSED) {
	struct page *item = hash_entry (e, struct page, hash_elem);
	uint64_t hash_va = hash_bytes (&item->va, sizeof item->va);

	return hash_va;
}

static bool
page_less (const struct hash_elem *a, const struct hash_elem *b,
		void *aux UNUSED) {
	struct page *page_one = hash_entry (a, struct page, hash_elem);
	struct page *page_two = hash_entry (b, struct page, hash_elem);
	return page_one->va < page_two->va;
}


/* 초기화 함수를 가진 대기 상태 페이지 객체를 만든다. 페이지가 필요하면 직접
 * 만들지 말고 이 함수나 vm_alloc_page()를 통해 만들어야 한다. */
bool
vm_alloc_page_with_initializer (enum vm_type type, void *upage, bool writable,
		vm_initializer *init, void *aux) {
			
	ASSERT(pg_ofs(upage) == 0);		
	ASSERT (VM_TYPE(type) != VM_UNINIT);

	struct supplemental_page_table *spt = &thread_current ()->spt;
	
	if (upage == NULL || spt_find_page (spt, upage) != NULL) 
		goto err;
		
		struct page* page = malloc(sizeof *page);
		if (page == NULL)
			goto err;
		
		bool (*initializer)(struct page *page, enum vm_type type, void *kva) = NULL;

		switch (VM_TYPE(type)) {
			case VM_ANON:
				initializer = anon_initializer;
				break;
			
			case VM_FILE:
				initializer = file_backed_initializer;
				break;
			
			default:
				free(page);
				goto err;
		}
		
		uninit_new(page, upage, init, type, aux, initializer);
		page->writable = writable;

		if(!spt_insert_page(spt, page)) {
			free(page);
			goto err;
		}
		else 
			return true;
		
err:
	return false;
}

/* Find VA from spt and return page. On error, return NULL. */
/* 페이지 폴트가 발생하면 spt 에서 va로 메타 데이터를 찾는 함수 */
struct page *
spt_find_page (struct supplemental_page_table *spt, void *va) {
	
	RETURN_VALUE_IF (spt == NULL || va == NULL, NULL);
	
	void * rounded_va = pg_round_down(va); 
	struct page temp;
	temp.va = rounded_va; 
	
	struct hash_elem * found = hash_find(&spt->hash_table, &temp.hash_elem);
	RETURN_VALUE_IF (found == NULL, NULL);

	struct page *found_page = hash_entry(found, struct page, hash_elem);
	return found_page;
}

/* Insert PAGE into spt with validation. */
bool
spt_insert_page (struct supplemental_page_table *spt,
		struct page *page) {

	RETURN_VALUE_IF (spt == NULL || page == NULL, false);
	ASSERT(pg_ofs(page->va) == 0);		
	
	return hash_insert(&spt->hash_table, &page->hash_elem) == NULL;
}

void
spt_remove_page (struct supplemental_page_table *spt, struct page *page) {
	RETURN_IF (spt == NULL || page == NULL);

	hash_delete (&spt->hash_table, &page->hash_elem);
	vm_dealloc_page (page);

	// return true;
}

/* 교체 대상이 될 struct frame을 고른다. */
static struct frame *
vm_get_victim (void) {
	struct frame *victim = NULL;
	/* TODO: 교체 정책은 직접 정한다. */

	return victim;
}

/* 페이지 하나를 내보내고 그 페이지에 대응하던 프레임을 반환한다.
 * 오류가 있으면 NULL을 반환한다. */
static struct frame *
vm_evict_frame (void) {
	struct frame *victim UNUSED = vm_get_victim ();
	/* TODO: swap out the victim and return the evicted frame. */

	return NULL;
}

/* palloc()을 호출해서 프레임을 얻는다.
 * 사용 가능한 페이지가 없으면, 기존 페이지 하나를 eviction해서
 * 그 프레임을 반환한다.
 *
 * 이 함수는 항상 유효한 주소를 반환한다.
 * 즉, user pool 메모리가 가득 차 있더라도 페이지를 eviction해서
 * 사용할 수 있는 메모리 공간을 확보한다.
 */
/* user pool 에서 공간 할당 받아오기
frame에 올리기 위해 할당받는곳 
 */
static struct frame *
vm_get_frame (void) {
	struct frame *frame = malloc(sizeof* frame);
	RETURN_VALUE_IF (frame == NULL, NULL);

	frame->kva = palloc_get_page (PAL_USER);
	if(frame->kva != NULL) {
		frame->page = NULL;
	}
	else {
		/*eviction 하는 코드 아직없움
		vm_evict_frame ();
		*/
		free(frame);
		return NULL;
	}
	/*
	list_init(&frame_table) vm 초기화 시점에 있어야함

	lock_acquire(&frame_table_lock)
	list_push_back(...)
	lock_release(&frame_table_lock)
	 */
	list_push_back(&frame_table, &frame->elem);

	// ASSERT (frame != NULL);
	// ASSERT (frame->page == NULL);

	// 만든 frame free는 언제하지 
	return frame;
}

/* 스택을 확장한다. */
static void
vm_stack_growth (void *addr UNUSED) {
	struct thread *curr = thread_current ();

	RETURN_IF (addr == NULL || curr == NULL || curr->stack_bottom == NULL);

	void *stack_bottom = curr->stack_bottom;
	void *page = pg_round_down (addr);

	while (page < stack_bottom) {
		stack_bottom -= PGSIZE;

		RETURN_IF (!vm_alloc_page (VM_ANON | VM_MARKER_0, stack_bottom, true));
		RETURN_IF (!vm_claim_page (stack_bottom));

		curr->stack_bottom = stack_bottom;
	}
}

/* 쓰기 보호 페이지에서 발생한 폴트를 처리한다. */
static bool
vm_handle_wp (struct page *page UNUSED) {
	return false;
}

// 수정필요 
bool
vm_try_handle_fault (struct intr_frame *f UNUSED, void *addr UNUSED,
                     bool user UNUSED, bool write UNUSED,
                     bool not_present UNUSED)
{
  struct supplemental_page_table *spt UNUSED = &thread_current ()->spt;
  struct page *page = NULL;
  uintptr_t rsp = user ? f->rsp : thread_current ()->user_rsp;

  if (addr == NULL || !is_user_vaddr (addr) || not_present == false)
    {
      return false;
    }

  void *pg_addr = pg_round_down (addr);

  page = spt_find_page (spt, pg_addr);
  if (page != NULL)
    {
      if (page->frame != NULL)
        return true;
      return vm_do_claim_page (page);
    }
  if (addr >= rsp - 8 && addr > USER_STACK - (1 << 20) && addr < USER_STACK)
    {
      vm_stack_growth (addr);
      return true;
    }
  return false;
}

/* Free the page.
 * DO NOT MODIFY THIS FUNCTION. */
void
vm_dealloc_page (struct page *page) {
	destroy (page);
	free (page);
}

/* Claim the page that allocate on VA. */
/* 인자로 받은 va에 해당하는 page를 SPT에서 찾고, 그 page를 실제 frame에 올리는 함수 */
bool
vm_claim_page (void *va) {
	struct thread *curr = thread_current ();
	RETURN_VALUE_IF (va == NULL || curr == NULL, false);

	struct supplemental_page_table *spt = &curr->spt;
	struct page* page = spt_find_page(spt, va);
	RETURN_VALUE_IF (page == NULL, false);

	return vm_do_claim_page (page);
	
}

/* PAGE를 실제 메모리에 올리고 MMU 매핑을 설정한다. */
static bool
vm_do_claim_page (struct page *page) {
	struct thread *curr = thread_current ();
	RETURN_VALUE_IF (page == NULL || curr == NULL, false);

	struct frame *frame = vm_get_frame ();
	RETURN_VALUE_IF(frame == NULL , false);

	frame->page = page;
	page->frame = frame;

	if (!pml4_set_page (curr->pml4, page->va, frame->kva, page->writable)) {
		frame->page = NULL;
		page->frame = NULL;
		palloc_free_page (frame->kva);
		free (frame);
		return false;
	}

	return swap_in (page, frame->kva);
}

/* Initialize new supplemental page table */
void
supplemental_page_table_init (struct supplemental_page_table *spt) {
	ASSERT(hash_init(&spt->hash_table, page_hash, page_less, NULL));
	ASSERT(hash_empty(&spt->hash_table));
}

/* Copy supplemental page table from src to dst */
bool
supplemental_page_table_copy (struct supplemental_page_table *dst UNUSED,
                              struct supplemental_page_table *src UNUSED) {

  struct hash_iterator i;
  hash_first (&i, &src->hash_table);
  while (hash_next (&i))
    {
      struct page *f = hash_entry (hash_cur (&i), struct page, hash_elem);
      enum vm_type page_type = page_get_type (f);
      if (page_type == VM_UNINIT)
        {
          void *aux = f->uninit.aux;
          if (aux)
            {
              struct lazy_load_arg *temp_aux
                  = (struct lazy_load_arg *)malloc (sizeof (struct lazy_load_arg));
              memcpy (temp_aux, f->uninit.aux, sizeof (struct lazy_load_arg));
              if (temp_aux->file)
                {
                  temp_aux->file = file_reopen (temp_aux->file);
                  if (temp_aux->file == NULL)
                    {
                      free (temp_aux);
                      return false;
                    }
                }
              aux = temp_aux;
            }

          if (!vm_alloc_page_with_initializer (
                  f->uninit.type, f->va, f->writable,
                  f->uninit.page_initializer, aux))
            return false;
        }
      else
        {
          if (!vm_alloc_page (page_type, f->va, f->writable))
            return false;
          struct page *child_page = spt_find_page (dst, f->va);
          if (child_page == NULL)
            return false;
          if (!vm_do_claim_page (child_page))
            return false;
          memcpy (child_page->frame->kva, f->frame->kva, PGSIZE);
        }
    }
  return true;
}

/* Free the resource hold by the supplemental page table */
void
supplemental_page_table_kill (struct supplemental_page_table *spt UNUSED) {
	/* TODO: Destroy all the supplemental_page_table hold by thread and
	 * TODO: writeback all the modified contents to the storage. */
}
