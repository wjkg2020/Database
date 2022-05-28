#include <algorithm>
#include "index/basic_comparator.h"
#include "index/generic_key.h"
#include "page/b_plus_tree_leaf_page.h"
#include "include/page/b_plus_tree_internal_page.h"

/*****************************************************************************
 * HELPER METHODS AND UTILITIES
 *****************************************************************************/

/**
 * Init method after creating a new leaf page
 * Including set page type, set current size to zero, set page id/parent id, set
 * next page id and set max size
 */
INDEX_TEMPLATE_ARGUMENTS
void B_PLUS_TREE_LEAF_PAGE_TYPE::Init(page_id_t page_id, page_id_t parent_id, int max_size) {
  SetPageType(IndexPageType::LEAF_PAGE);
  SetNextPageId(INVALID_PAGE_ID);
  SetSize(0);
  SetMaxSize((PAGE_SIZE-sizeof(BPlusTreeLeafPage))/sizeof(MappingType)-1);
  SetPageId(page_id);
  SetParentPageId(parent_id);
  this->array_=new MappingType[max_size_+1];
  return;
}



/**
 * Helper methods to set/get next page id
 */
INDEX_TEMPLATE_ARGUMENTS
page_id_t B_PLUS_TREE_LEAF_PAGE_TYPE::GetNextPageId() const {
  return next_page_id_;
}

INDEX_TEMPLATE_ARGUMENTS
void B_PLUS_TREE_LEAF_PAGE_TYPE::SetNextPageId(page_id_t next_page_id) {
  next_page_id_=next_page_id;
}

/**
 * Helper method to find the first index i so that array_[i].first >= key
 * NOTE: This method is only used when generating index iterator
 */
INDEX_TEMPLATE_ARGUMENTS
int B_PLUS_TREE_LEAF_PAGE_TYPE::KeyIndex(const KeyType &key, const KeyComparator &comparator) const {
  int l=0,r=GetSize()-1;
  while(l<=r){
    int mid=(l+r)/2;
    if(comparator(array_[mid].first,key)>=0) r=mid-1;
    else l=mid+1;
  }
  return r+1;
}

/*
 * Helper method to find and return the key associated with input "index"(a.k.a
 * array offset)
 */
INDEX_TEMPLATE_ARGUMENTS
KeyType B_PLUS_TREE_LEAF_PAGE_TYPE::KeyAt(int index) const {
  return array_[index].first;
}

/*
 * Helper method to find and return the key & value pair associated with input
 * "index"(a.k.a array offset)
 */
INDEX_TEMPLATE_ARGUMENTS
const MappingType &B_PLUS_TREE_LEAF_PAGE_TYPE::GetItem(int index) {
  // replace with your own code
  return array_[index];
}

/*****************************************************************************
 * INSERTION
 *****************************************************************************/
/*
 * Insert key & value pair into leaf page ordered by key
 * @return page size after insertion
 */
INDEX_TEMPLATE_ARGUMENTS
int B_PLUS_TREE_LEAF_PAGE_TYPE::Insert(const KeyType &key, const ValueType &value, const KeyComparator &comparator) {
  int index=keyindex(key,comparator);
  IncreaseSize(1);
  int now_size=GetSize();
  for(int i=now_size-1;i>index;i--){
    array_[i]=array_[i-1];
  }
  array_[index].first=key;
  array_[index].second=value;
  return now_size;
}

/*****************************************************************************
 * SPLIT
 *****************************************************************************/
/*
 * Remove half of key & value pairs from this page to "recipient" page
 */
INDEX_TEMPLATE_ARGUMENTS
void B_PLUS_TREE_LEAF_PAGE_TYPE::MoveHalfTo(BPlusTreeLeafPage *recipient) {
  int now_size=GetMaxSize()+1;
  int break_index=now_size/2;
  for(int i=break_index;i<now_size;i++){
    recipient->array_[i-break_index]=array_[i];
  }
  recipient->SetNextPageId(GetNextPageId());
  SetNextPageId(recipient->GetPageId());
  SetSize(break_index);
  recipient->SetSize(now_size-break_index);
}



/*
 * Copy starting from items, and copy {size} number of elements into me.
 */
INDEX_TEMPLATE_ARGUMENTS
void B_PLUS_TREE_LEAF_PAGE_TYPE::CopyNFrom(MappingType *items, int size) {
  for(int i=0;i<size;i++){
    this->array_[i]=items[i];
  }
}

/*****************************************************************************
 * LOOKUP
 *****************************************************************************/
/*
 * For the given key, check to see whether it exists in the leaf page. If it
 * does, then store its corresponding value in input "value" and return true.
 * If the key does not exist, then return false
 */
INDEX_TEMPLATE_ARGUMENTS
bool B_PLUS_TREE_LEAF_PAGE_TYPE::Lookup(const KeyType &key, ValueType &value, const KeyComparator &comparator) const {
  int index=KeyIndex(key,comparator);
  if(index<GetSize()&&comparator(array_[index].first,key)==0){
    value=array_[index].second;
    return true;
  }
  return false;
}

/*****************************************************************************
 * REMOVE
 *****************************************************************************/
/*
 * First look through leaf page to see whether delete key exist or not. If
 * exist, perform deletion, otherwise return immediately.
 * NOTE: store key&value pair continuously after deletion
 * @return  page size after deletion
 */
INDEX_TEMPLATE_ARGUMENTS
int B_PLUS_TREE_LEAF_PAGE_TYPE::RemoveAndDeleteRecord(const KeyType &key, const KeyComparator &comparator) {
  int dayudengyu_index=KeyIndex(key,comparator);
  if(dayudengyu_index>=GetSize()||comparator(key,KeyAt(dayudengyu_index))!=0){
    return GetSize();
  }
  memmove(array_+dayudengyu_index,array_+dayudengyu_index+1,
          static_cast<size_t>((GetSize()-dayudengyu_index-1)*sizeof(MappingType)));
  IncreaseSize(-1);
  return GetSize();
}

/*****************************************************************************
 * MERGE
 *****************************************************************************/
/*
 * Remove all of key & value pairs from this page to "recipient" page. Don't forget
 * to update the next_page id in the sibling page
 */
INDEX_TEMPLATE_ARGUMENTS
void B_PLUS_TREE_LEAF_PAGE_TYPE::MoveAllTo(BPlusTreeLeafPage *recipient) {
  int startIdx = recipient->GetSize();
  for (int i = 0; i < GetSize(); i++) {
    recipient->array_[startIdx + i].first = array_[i].first;
    recipient->array_[startIdx + i].second = array_[i].second;
  }
  recipient->SetNextPageId(GetNextPageId());
  recipient->IncreaseSize(GetSize());
  SetSize(0);
}

/*****************************************************************************
 * REDISTRIBUTE
 *****************************************************************************/
/*
 * Remove the first key & value pair from this page to "recipient" page.
 *
 */
INDEX_TEMPLATE_ARGUMENTS
void B_PLUS_TREE_LEAF_PAGE_TYPE::MoveFirstToEndOf(BPlusTreeLeafPage *recipient) {
  MappingType pair = GetItem(0);
  IncreaseSize(-1);
  memmove(array_, array_ + 1, static_cast<size_t>(GetSize()*sizeof(MappingType)));
  recipient->CopyLastFrom(pair);
  //update relavent key & value pair in its parent page.
  Page *page = buffer_pool_manager->FetchPage(GetParentPageId());
  B_PLUS_TREE_INTERNAL_PAGE *parent = reinterpret_cast<B_PLUS_TREE_INTERNAL_PAGE *>(page->GetData());
  parent->SetKeyAt(parent->ValueIndex(GetPageId()), array_[0].first);
  buffer_pool_manager->UnpinPage(GetParentPageId(), true);
}

/*
 * Copy the item into the end of my item list. (Append item to my array)
 */
INDEX_TEMPLATE_ARGUMENTS
void B_PLUS_TREE_LEAF_PAGE_TYPE::CopyLastFrom(const MappingType &item) {
  array_[GetSize()] = item;
  IncreaseSize(1);
}

/*
 * Remove the last key & value pair from this page to "recipient" page.
 */
INDEX_TEMPLATE_ARGUMENTS
void B_PLUS_TREE_LEAF_PAGE_TYPE::MoveLastToFrontOf(BPlusTreeLeafPage *recipient) { {
  MappingType pair = GetItem(GetSize() - 1);
  IncreaseSize(-1);
  recipient->CopyFirstFrom(pair, parentIndex, buffer_pool_manager);
}

/*
 * Insert item at the front of my items. Move items accordingly.
 *
 */
INDEX_TEMPLATE_ARGUMENTS
void B_PLUS_TREE_LEAF_PAGE_TYPE::CopyFirstFrom(const MappingType &item) {
  memmove(array + 1, array, GetSize()*sizeof(MappingType));
  IncreaseSize(1);
  array[0] = item;

  Page *page = buffer_pool_manager->FetchPage(GetParentPageId());
  B_PLUS_TREE_INTERNAL_PAGE_TYPE *parent = reinterpret_cast<B_PLUS_TREE_INTERNAL_PAGE_TYPE *>(page->GetData());
  parent->SetKeyAt(parent->KeyIndex(array_[1].first), array_[0].first);
  buffer_pool_manager->UnpinPage(GetParentPageId(), true);
}

template
class BPlusTreeLeafPage<int, int, BasicComparator<int>>;

template
class BPlusTreeLeafPage<GenericKey<4>, RowId, GenericComparator<4>>;

template
class BPlusTreeLeafPage<GenericKey<8>, RowId, GenericComparator<8>>;

template
class BPlusTreeLeafPage<GenericKey<16>, RowId, GenericComparator<16>>;

template
class BPlusTreeLeafPage<GenericKey<32>, RowId, GenericComparator<32>>;

template
class BPlusTreeLeafPage<GenericKey<64>, RowId, GenericComparator<64>>;