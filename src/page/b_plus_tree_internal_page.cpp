#include "index/basic_comparator.h"
#include "index/generic_key.h"
#include "page/b_plus_tree_internal_page.h"

/*****************************************************************************
 * HELPER METHODS AND UTILITIES
 *****************************************************************************/
/*
 * Init method after creating a new internal page
 * Including set page type, set current size, set page id, set parent id and set
 * max page size
 */
INDEX_TEMPLATE_ARGUMENTS
void B_PLUS_TREE_INTERNAL_PAGE_TYPE::Init(page_id_t page_id, page_id_t parent_id, int max_size) {
    SetPageType(IndexPageType::INTERNAL_PAGE);
    SetSize(0);
    SetPageId(page_id);
    SetParentPageId(parent_id);
    SetMaxSize((PAGE_SIZE-sizeof(BPlusTreeInternalPage))/sizeof(MappingType)-1);

    this->array_=new MappingType[max_size];
}
/*
 * Helper method to get/set the key associated with input "index"(a.k.a
 * array offset)
 */
INDEX_TEMPLATE_ARGUMENTS
KeyType B_PLUS_TREE_INTERNAL_PAGE_TYPE::KeyAt(int index) const {
  // replace with your own code
  KeyType key=this->array_[index].first;
  return key;
}

INDEX_TEMPLATE_ARGUMENTS
void B_PLUS_TREE_INTERNAL_PAGE_TYPE::SetKeyAt(int index, const KeyType &key) {
  this->array_[index].first=key;
}

/*
 * Helper method to find and return array index(or offset), so that its value
 * equals to input "value"
 */
INDEX_TEMPLATE_ARGUMENTS
int B_PLUS_TREE_INTERNAL_PAGE_TYPE::ValueIndex(const ValueType &value) const {
  int size=GetSize();
  for(int i=1;i<size;i++){
    if(this->array_[i].second==value) {
      return i;
    }
  }
  return -1;
}

INDEX_TEMPLATE_ARGUMENTS
int B_PLUS_TREE_INTERNAL_PAGE_TYPE::KeyIndex(const KeyType &key) const {
  int size=GetSize();
  for(int i=0;i<size;i++){
    if(this->array_[i].first==key) {
      return i;
    }
  }
  return -1;
}

/*
 * Helper method to get the value associated with input "index"(a.k.a array
 * offset)
 */
INDEX_TEMPLATE_ARGUMENTS
ValueType B_PLUS_TREE_INTERNAL_PAGE_TYPE::ValueAt(int index) const {
  ValueType val=this->array_[index].second;
  return val;
}

/*****************************************************************************
 * LOOKUP
 *****************************************************************************/
/*
 * Find and return the child pointer(page_id) which points to the child page
 * that contains input "key"
 * Start the search from the second key(the first key should always be invalid)
 */
INDEX_TEMPLATE_ARGUMENTS
ValueType B_PLUS_TREE_INTERNAL_PAGE_TYPE::Lookup(const KeyType &key, const KeyComparator &comparator) const {
  assert(GetSize()>1);
  if(comparator(key,array_[1].first)<0){
    return array_[0].second;
  }
  if(comparator(key,array_[GetSize()-1].first)>=0){
    return array_[GetSize()-1].second;
  }
  int left=1,right=GetSize()-1;
  int targetIndex=-1;
  while(left<=right){
    int mid=left+(right-left)/2;
    int compareResult=comparator(array_[mid].first,key);
    if(compareResult==0){
      targetIndex=mid;
      break;
    }
    if(compareResult<0){
      left=mid+1;
    }else{
      right=mid+1;
    }
  }
  if(targetIndex!=-1){
    return array_[targetIndex].second;
  }
  return array_[left-1].second;
}

/*****************************************************************************
 * INSERTION
 *****************************************************************************/
/*
 * Populate new root page with old_value + new_key & new_value
 * When the insertion cause overflow from leaf page all the way upto the root
 * page, you should create a new root page and populate its elements.
 * NOTE: This method is only called within InsertIntoParent()(b_plus_tree.cpp)
 */
INDEX_TEMPLATE_ARGUMENTS
void B_PLUS_TREE_INTERNAL_PAGE_TYPE::PopulateNewRoot(const ValueType &old_value, const KeyType &new_key,const ValueType &new_value) {
          array_[0].second=old_value;
          array_[1].first=new_key;
          array_[1].second=new_value;
          SetSize(2);
}

/*
 * Insert new_key & new_value pair right after the pair with its value ==
 * old_value
 * @return:  new size after insertion
 */
INDEX_TEMPLATE_ARGUMENTS
int B_PLUS_TREE_INTERNAL_PAGE_TYPE::InsertNodeAfter(const ValueType &old_value, const KeyType &new_key,const ValueType &new_value) {
  int index=ValueIndex(old_value);
  assert(index!=-1);
  for(int i=GetSize()-1;i>index;i--){
    array_[i+1]=array_[i];
  }
  array_[index+1].first=new_key;
  array_[index+1].second=new_value;
  IncreaseSize(1);
  return GetSize();
}

/*****************************************************************************
 * SPLIT
 *****************************************************************************/
/*
 * Remove half of key & value pairs from this page to "recipient" page
 */
INDEX_TEMPLATE_ARGUMENTS
void B_PLUS_TREE_INTERNAL_PAGE_TYPE::MoveHalfTo(BPlusTreeInternalPage *recipient,BufferPoolManager *buffer_pool_manager) {
    int size=GetSize();
    int half_size=size/2;
    MappingType *st=array_+size-half_size;
    recipient->CopyFirstFrom(st,half_size);
    this->IncreaseSize(-1*half_size);
    recipient->IncreaseSize(half_size);
    return;
}

/* Copy entries into me, starting from {items} and copy {size} entries.
 * Since it is an internal page, for all entries (pages) moved, their parents page now changes to me.
 * So I need to 'adopt' them by changing their parent page id, which needs to be persisted with BufferPoolManger
 */
INDEX_TEMPLATE_ARGUMENTS
void B_PLUS_TREE_INTERNAL_PAGE_TYPE::CopyNFrom(MappingType *items, int size, BufferPoolManager *buffer_pool_manager) {
    for(int i=0;i<size;i++){
      this->array_[i]=item[i];
      auto new_page=reinterpret_cast<BPlusTreePage *>(buffer_pool_manager->FetchPage(this->array_[i].second)->GetData());
      new_page->SetParentPageId(this->GetPageId());
      buffer_pool_manager->UnpinPage(this->array_[i].second,true);
    }
}

/*****************************************************************************
 * REMOVE
 *****************************************************************************/
/*
 * Remove the key & value pair in internal page according to input index(a.k.a
 * array offset)
 * NOTE: store key&value pair continuously after deletion
 */
INDEX_TEMPLATE_ARGUMENTS
void B_PLUS_TREE_INTERNAL_PAGE_TYPE::Remove(int index) {
    memmove(array_+index,array_+index+1,(GetSize()-index-1)*sizeof(MappingType)));
    IncreaseSize(-1);
    return;
}

/*
 * Remove the only key & value pair in internal page and return the value
 * NOTE: only call this method within AdjustRoot()(in b_plus_tree.cpp)
 */
INDEX_TEMPLATE_ARGUMENTS
ValueType B_PLUS_TREE_INTERNAL_PAGE_TYPE::RemoveAndReturnOnlyChild() {
  ValueType val=this->ValueAt(0);
  IncreaseSize(-1);
  assert(GetSize()!=0)
  return val;
}

/*****************************************************************************
 * MERGE
 *****************************************************************************/
/*
 * Remove all of key & value pairs from this page to "recipient" page.
 * The middle_key is the separation key you should get from the parent. You need
 * to make sure the middle key is added to the recipient to maintain the invariant.
 * You also need to use BufferPoolManager to persist changes to the parent page id for those
 * pages that are moved to the recipient
 */
INDEX_TEMPLATE_ARGUMENTS
void B_PLUS_TREE_INTERNAL_PAGE_TYPE::MoveAllTo(BPlusTreeInternalPage *recipient, const KeyType &middle_key,BufferPoolManager *buffer_pool_manager) {
    int re_size=recipient->GetSize();
    page_id_t re_page_id=recipient->page_id_;
    Page *page=buffer_pool_manager->FetchPage(this->GetParentPageId());
    BPlusTreeInternalPage *father=reinterpret_cast<BPlusTreeInternalPage *>(page->GetData());
    int index=father->ValueIndex(this->GetPageId);
    KeyType tmp=father->KeyAt(index);
    this->SetKeyAt(0,tmp);
    father->Remove(index);
    buffer_pool_manager->UnpinPage(father->GetPageId(),true);
    recipient->CopyNFrom(array_,GetSize(),buffer_pool_manager);
    SetSize(0);
}

/*****************************************************************************
 * REDISTRIBUTE
 *****************************************************************************/
/*
 * Remove the first key & value pair from this page to tail of "recipient" page.
 *
 * The middle_key is the separation key you should get from the parent. You need
 * to make sure the middle key is added to the recipient to maintain the invariant.
 * You also need to use BufferPoolManager to persist changes to the parent page id for those
 * pages that are moved to the recipient
 */
INDEX_TEMPLATE_ARGUMENTS
void B_PLUS_TREE_INTERNAL_PAGE_TYPE::MoveFirstToEndOf(BPlusTreeInternalPage *recipient, const KeyType &middle_key,BufferPoolManager *buffer_pool_manager) {
  auto *fatherData=buffer_pool_manager->FetchPage(GetParentPageId)->GetData();
  B_PLUS_TREE_INTERNAL_PAGE_TYPE *fatherpage=reinterpret_cast<B_PLUS_TREE_INTERNAL_PAGE_TYPE*>(fatherData)
  int index=fatherpage->ValueIndex(GetPageId());
  KeyType tmp=fatherpage->KeyAt(index);
  buffer_pool_manager->UnpinPage(GetParentPageId(),true);
  MappingType new_pair={tmp,ValueAt(0)};
  (this->array_[0]).second=this->ValueAt(1);
  this->Remove(1);
  recipient->CopyLastFrom(new_pair,buffer_pool_manager);
  IncreaseSize(-1);
  return;
}

/* Append an entry at the end.
 * Since it is an internal page, the moved entry(page)'s parent needs to be updated.
 * So I need to 'adopt' it by changing its parent page id, which needs to be persisted with BufferPoolManger
 */
INDEX_TEMPLATE_ARGUMENTS
void B_PLUS_TREE_INTERNAL_PAGE_TYPE::CopyLastFrom(const MappingType &pair, BufferPoolManager *buffer_pool_manager) {
  auto *childData=buffer_pool_manager->FetchPage(pair.second)->GetData();
  B_PLUS_TREE_INTERNAL_PAGE_TYPE *childPage=reinterpret_cast<B_PLUS_TREE_INTERNAL_PAGE_TYPE *>(childData);
  childPage->SetPageId(GetPageId());
  buffer_pool_manager->UnpinPage(pair.second,true);
  array[GetSize()]=pair;
  IncreaseSize(1);
}

/*
 * Remove the last key & value pair from this page to head of "recipient" page.
 * You need to handle the original dummy key properly, e.g. updating recipient’s array to position the middle_key at the
 * right place.
 * You also need to use BufferPoolManager to persist changes to the parent page id for those pages that are
 * moved to the recipient
 */
INDEX_TEMPLATE_ARGUMENTS
void B_PLUS_TREE_INTERNAL_PAGE_TYPE::MoveLastToFrontOf(BPlusTreeInternalPage *recipient, const KeyType &middle_key,BufferPoolManager *buffer_pool_manager) {
  MappingType movepair=array_[GetSize()-1];
  IncreaseSize(-1);
  recipient->CopyFirstFrom(movepair,buffer_pool_manager);
}

/* Append an entry at the beginning.
 * Since it is an internal page, the moved entry(page)'s parent needs to be updated.
 * So I need to 'adopt' it by changing its parent page id, which needs to be persisted with BufferPoolManger
 */
INDEX_TEMPLATE_ARGUMENTS
void B_PLUS_TREE_INTERNAL_PAGE_TYPE::CopyFirstFrom(const MappingType &pair, BufferPoolManager *buffer_pool_manager) {
    auto *fatherdata=buffer_pool_manager->FetchPage(GetParentPageId())->GetData();
    B_PLUS_TREE_INTERNAL_PAGE_TYPE * fatherpage=reinterpret_cast<B_PLUS_TREE_INTERNAL_PAGE_TYPE *>(fatherdata);
    int index=fatherpage->ValueIndex(GetPageId);
    SetKeyAt(0,fatherpage->KeyAt(index));
    fatherpage->SetKeyAt(0,pair.first);
    buffer_pool_manager->UnpinPage(fatherpage->GetPageId(),true);
    for(int i=GetSize()-1;i>=0;i--){
      array_[i+1]=array_[i];
    }
    array_[0].second=pair.second;
    auto * childdata=buffer_pool_manager->FetchPage(pair.second)->GetData();
    B_PLUS_TREE_INTERNAL_PAGE_TYPE *childpage=reinterpret_cast<B_PLUS_TREE_INTERNAL_PAGE_TYPE *>(childdata);
    childpage->SetParentPageId(GetPageId());
    buffer_pool_manager->UnpinPage(childpage->GetPageId(),true);
}

template
class BPlusTreeInternalPage<int, int, BasicComparator<int>>;

template
class BPlusTreeInternalPage<GenericKey<4>, page_id_t, GenericComparator<4>>;

template
class BPlusTreeInternalPage<GenericKey<8>, page_id_t, GenericComparator<8>>;

template
class BPlusTreeInternalPage<GenericKey<16>, page_id_t, GenericComparator<16>>;

template
class BPlusTreeInternalPage<GenericKey<32>, page_id_t, GenericComparator<32>>;

template
class BPlusTreeInternalPage<GenericKey<64>, page_id_t, GenericComparator<64>>;