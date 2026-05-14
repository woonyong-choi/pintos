/* vm.c: Generic interface for virtual memory objects. */

#include "threads/malloc.h"
#include "threads/vaddr.h"
#include "vm/vm.h"
#include "vm/inspect.h"
/* HOSEOK'S CODE */
#include "hash.h"

static uint64_t page_hash (const struct hash_elem *e, void *aux UNUSED);
static bool page_less (const struct hash_elem *a, const struct hash_elem *b, void *aux UNUSED);

static struct list frame_table; // 물리 공간에 할당된 frame 객체들을 관리하기 위한 전역변수 

/* Initializes the virtual memory subsystem by invoking each subsystem's
 * intialize codes. */
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

/* Get the type of the page. This function is useful if you want to know the
 * type of the page after it will be initialized.
 * This function is fully implemented now. */
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

/* Helpers */
static struct frame *vm_get_victim (void);
static bool vm_do_claim_page (struct page *page);
static struct frame *vm_evict_frame (void);

/* Create the pending page object with initializer. If you want to create a
 * page, do not create it directly and make it through this function or
 * `vm_alloc_page`. */
/* initializer를 가진 pending page 객체를 생성한다.
 * page를 만들고 싶다면 직접 만들지 말고,
 * 이 함수나 `vm_alloc_page`를 통해 만들어라. */

/* SPT에 “나중에 로드될 page 메타데이터”를 등록하는 함수 */
bool
vm_alloc_page_with_initializer (enum vm_type type, void *upage, bool writable,
		vm_initializer *init, void *aux) {
	ASSERT(pg_ofs(upage) == 0);		
	ASSERT (VM_TYPE(type) != VM_UNINIT);

	struct supplemental_page_table *spt = &thread_current ()->spt;
	
	if (upage == NULL)
		goto err;
	/* Check wheter the upage is already occupied or not. */
	/*2. upage가 이미 spt에 등록되어있는지 확인한다. */
	if (spt_find_page (spt, upage) == NULL) {
		/* TODO: 페이지를 생성하고, VM 타입에 맞는 initializer를 가져온다.
		 * TODO: 그런 다음 uninit_new를 호출해서 "uninit" page 구조체를 생성한다.
 		 * TODO: uninit_new를 호출한 뒤에는 필요한 필드를 수정해야 한다. */
		 /* TODO: page를 spt에 삽입한다. */

		 /* 3. page를 동적할당한다. */
		struct page* page = malloc(sizeof *page);
		if (page == NULL)
			goto err;
		/* 4. anon타입, file타입을 정해준다*/
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
		page->rw = writable;

		if(!spt_insert_page(spt, page)) {
			free(page);
			goto err;
		}
		else {
			return true;
		}
	}
err:
	return false;
}

/* Find VA from spt and return page. On error, return NULL. */
/* 페이지 폴트가 발생하면 spt 에서 va로 메타 데이터를 찾는 함수 */
struct page *
spt_find_page (struct supplemental_page_table *spt, void *va) {
	
	/* TODO: Fill this function. */
	// 1. pg_round_down 으로 페이지폴트 난 주소에서부터 첫 page의 주소를 찾는다. 
	void * rounded_va = pg_round_down(va); // 0x411234 -> 0x411000
	struct page temp;
	temp.va = rounded_va; 
	// 2. spt 에 접근해 현재 va에 있는지 확인하고 있으면 접근 없으면 nopage(invaild access) 종료한다.  
	// 2.1 접근 하는 방법이 뭐지 spt와 va 가 있으니까 spt안에 hash 구조체 만들고 그안에 bucket에 인덱스 조사하면 되나 -> // page 구조체에 hash_elem 타입의 hash_elem 선언
	struct hash_elem * elem = hash_find(&spt->hash_table, &temp.hash_elem);
	if (elem == NULL) {
		return NULL;
	}
	
	struct page *found_page = hash_entry(elem, struct page, hash_elem);
	return found_page;
}

/* Insert PAGE into spt with validation. */
bool
spt_insert_page (struct supplemental_page_table *spt,
		struct page *page) {
	int succ = false;
	/* TODO: Fill this function. */
	/* 받은 page를 hash_insert 해준다 */
	/* 필요한거 1. hash 구조체 2. page의 hash_elem */
	
	/* hash_insert 반환값이 NULL 이면 삽입 성공 */
	if (hash_insert (&spt->hash_table, &page->hash_elem) == NULL) {
		succ = true;
	}
	return succ;
}

void
spt_remove_page (struct supplemental_page_table *spt, struct page *page) {
	hash_delete (&spt->hash_table, &page->hash_elem);
	vm_dealloc_page (page);

	// return true;
}

/* Get the struct frame, that will be evicted. */
static struct frame *
vm_get_victim (void) {
	struct frame *victim = NULL;
	 /* TODO: The policy for eviction is up to you. */

	return victim;
}

/* Evict one page and return the corresponding frame.
 * Return NULL on error.*/
static struct frame *
vm_evict_frame (void) {
	struct frame *victim UNUSED = vm_get_victim ();
	/* TODO: swap out the victim and return the evicted frame. */

	return NULL;
}

/* palloc() and get frame. If there is no available page, evict the page
 * and return it. This always return valid address. That is, if the user pool
 * memory is full, this function evicts the frame to get the available memory
 * space.*/
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
	struct frame *frame;
	/* TODO: Fill this function. */
	// 왜 palloc도 하고 malloc도 하라는거지 
	// page와 frame은 양방향 선언되있음 
	// page->frame: page fault 처리중 이 page가 어느 frame에 올라갔는지 알아야함 
	// frame->page: evction 처리, 어느 page를 쫒을지 고민하기 위해 알아야함
	frame = malloc(sizeof* frame);
	if (frame == NULL) {
		return NULL; 
	}

	void *kva = palloc_get_page(PAL_USER); 
	if(kva != NULL) {
		frame->kva = kva;
		frame->page = NULL;
	}
	else {
		/*eviction 하는 코드 */
		/*eviction 시도는 어떻게 하지 */
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

/* Growing the stack. */
static void
vm_stack_growth (void *addr UNUSED) {
}

/* Handle the fault on write_protected page */
static bool
vm_handle_wp (struct page *page UNUSED) {
	return false;
}

/* Return true on success */
bool
vm_try_handle_fault (struct intr_frame *f UNUSED, void *addr UNUSED,
		bool user UNUSED, bool write UNUSED, bool not_present UNUSED) {
	struct supplemental_page_table *spt UNUSED = &thread_current ()->spt;
	struct page *page = NULL;
	/* TODO: Validate the fault */
	/* TODO: Your code goes here */

	return vm_do_claim_page (page);
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
	struct page *page;

	page = spt_find_page(&thread_current()->spt, va);

	/* TODO: Fill this function */
	/* 현재 스레드의 spt에 접근해서 인자로 받은 va값에 대응되는 버켓이 있는지 확인?*/
	if (page != NULL) {
		return vm_do_claim_page (page);
	}
	else {
		return false;
	}
	
}

/* Claim the PAGE and set up the mmu. */
/* PAGE를 claim하고 MMU를 설정한다. */
/* 해당 page를 현재 프로세스가 실제로 사용할 수 있게 확보하고, 
MMU가 그 페이지를 올바르게 주소 변환할 수 있도록 페이지 테이블 같은 메모리 매핑 정보를 설정한다는 뜻*/
static bool
vm_do_claim_page (struct page *page) {
	/* 확보 */
	struct frame *frame = vm_get_frame ();

	/* Set links */
	frame->page = page;
	page->frame = frame;

	pml4_set_page (thread_current()->pml4, page->va, frame->kva, page->rw);
	/* TODO: Insert page table entry to map page's VA to frame's PA. */
	/* TODO: page의 VA를 frame의 PA에 매핑하는 페이지 테이블 엔트리를 삽입한다. */
	
	
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
	return false;
}

/* Free the resource hold by the supplemental page table */
void
supplemental_page_table_kill (struct supplemental_page_table *spt UNUSED) {
	/* TODO: Destroy all the supplemental_page_table hold by thread and
	 * TODO: writeback all the modified contents to the storage. */
}

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
