/* vm.c: Generic interface for virtual memory objects. */

#include "threads/malloc.h"
#include "vm/vm.h"
#include "vm/inspect.h"
/* HOSEOK'S CODE */
#include "hash.h"
static uint64_t test_hash (const struct hash_elem *e, void *aux UNUSED);
static bool test_less (const struct hash_elem *a,
		const struct hash_elem *b, void *aux UNUSED);
static void hash_self_test (void);
static uint64_t page_hash (const struct hash_elem *e);


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
	hash_self_test (); 
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
bool
vm_alloc_page_with_initializer (enum vm_type type, void *upage, bool writable,
		vm_initializer *init, void *aux) {

	ASSERT (VM_TYPE(type) != VM_UNINIT)

	struct supplemental_page_table *spt = &thread_current ()->spt;

	/* Check wheter the upage is already occupied or not. */
	if (spt_find_page (spt, upage) == NULL) {
		/* TODO: Create the page, fetch the initialier according to the VM type,
		 * TODO: and then create "uninit" page struct by calling uninit_new. You
		 * TODO: should modify the field after calling the uninit_new. */

		/* TODO: Insert the page into the spt. */
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
	void * rounded_va = pg_round_down(va);
	struct page temp;
	temp.va = rounded_va; 
	// 2. spt 에 접근해 현재 va에 있는지 확인하고 있으면 접근 없으면 nopage(invaild access) 종료한다.  
	// 2.1 접근 하는 방법이 뭐지 spt와 va 가 있으니까 spt안에 hash 구조체 만들고 그안에 bucket에 인덱스 조사하면 되나 
	// 근데 그렇게 설계하면 다른 elem 쓰는 리스트,ex)스레드 와 겹칠수 있지않나
	// page 구조체에 hash_elem 타입의 hash_elem 선언 
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
	/* 필요한거 1. hash 구조체 2. table의 hash_elem */
	
	/* hash_insert 반환값이 NULL 이면 삽입 성공 */
	if (hash_insert (&spt->hash_table, &page->hash_elem) == NULL) {
		succ = true;
	}
	return succ;
}

void
spt_remove_page (struct supplemental_page_table *spt, struct page *page) {
	vm_dealloc_page (page);
	return true;
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
static struct frame *
vm_get_frame (void) {
	struct frame *frame = NULL;
	/* TODO: Fill this function. */

	ASSERT (frame != NULL);
	ASSERT (frame->page == NULL);
	return frame;
}

/* Growing the stack. */
static void
vm_stack_growth (void *addr UNUSED) {
}

/* Handle the fault on write_protected page */
static bool
vm_handle_wp (struct page *page UNUSED) {
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
bool
vm_claim_page (void *va UNUSED) {
	struct page *page = NULL;
	/* TODO: Fill this function */

	return vm_do_claim_page (page);
}

/* Claim the PAGE and set up the mmu. */
static bool
vm_do_claim_page (struct page *page) {
	struct frame *frame = vm_get_frame ();

	/* Set links */
	frame->page = page;
	page->frame = frame;

	/* TODO: Insert page table entry to map page's VA to frame's PA. */

	return swap_in (page, frame->kva);
}

/* Initialize new supplemental page table */
void
supplemental_page_table_init (struct supplemental_page_table *spt) {
	struct hash h;

	ASSERT(hash_init(&h, page_hash, page_less, NULL));
	ASSERT(hash_empty(&h)); 

}

/* Copy supplemental page table from src to dst */
bool
supplemental_page_table_copy (struct supplemental_page_table *dst UNUSED,
		struct supplemental_page_table *src UNUSED) {
}

/* Free the resource hold by the supplemental page table */
void
supplemental_page_table_kill (struct supplemental_page_table *spt UNUSED) {
	/* TODO: Destroy all the supplemental_page_table hold by thread and
	 * TODO: writeback all the modified contents to the storage. */
}

static uint64_t
page_hash (const struct hash_elem *e) {
	struct page *item = hash_entry (e, struct page, hash_elem);
	uint64_t hash_va = hash_int (item->va);

	return hash_va;
}

static bool
page_less (const struct hash_elem *a, const struct hash_elem *b,
		void *aux UNUSED) {
	struct page *page_one = hash_entry (a, struct page, hash_elem);
	struct page *page_two = hash_entry (b, struct page, hash_elem);
	return page_one->va < page_two->va;
}

