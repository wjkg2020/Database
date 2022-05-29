#include <string>
#include "glog/logging.h"
#include "index/b_plus_tree.h"
#include "index/basic_comparator.h"
#include "index/generic_key.h"
#include "page/index_roots_page.h"

INDEX_TEMPLATE_ARGUMENTS
BPLUSTREE_TYPE::BPlusTree(index_id_t index_id, BufferPoolManager *buffer_pool_manager, const KeyComparator &comparator,
                          int leaf_max_size, int internal_max_size)
        : index_id_(index_id),
          buffer_pool_manager_(buffer_pool_manager),
          comparator_(comparator),
          leaf_max_size_(leaf_max_size),
          internal_max_size_(internal_max_size) {

}

INDEX_TEMPLATE_ARGUMENTS
void BPLUSTREE_TYPE::Destroy() {

}

/*
 * Helper function to decide whether current b+tree is empty
 */
INDEX_TEMPLATE_ARGUMENTS
bool BPLUSTREE_TYPE::IsEmpty() const {
  if(root_page_id_==INVALID_PAGE_ID) return true;
  return false;
}

/*****************************************************************************
 * SEARCH
 *****************************************************************************/
/*
 * Return the only value that associated with input key
 * This method is used for point query
 * @return : true means key exists
 */
INDEX_TEMPLATE_ARGUMENTS
bool BPLUSTREE_TYPE::GetValue(const KeyType &key, std::vector<ValueType> &result, Transaction *transaction) {
   B_PLUS_TREE_LEAF_PAGE_TYPE *target=FindLeafPage(key,false);
   if(target==nullptr) return false;
   result.resize(1);
   bool suc=target->Lookup(key,result[0],comparator_);
   buffer_pool_manager_->UnpinPage(target->GetPageId(),false);
   return suc;
}

/*****************************************************************************
 * INSERTION
 *****************************************************************************/
/*
 * Insert constant key & value pair into b+ tree
 * if current tree is empty, start new tree, update root page id and insert
 * entry, otherwise insert into leaf page.
 * @return: since we only support unique key, if user try to insert duplicate
 * keys return false, otherwise return true.
 */
INDEX_TEMPLATE_ARGUMENTS
bool BPLUSTREE_TYPE::Insert(const KeyType &key, const ValueType &value, Transaction *transaction) {
  if(IsEmpty()) {
    StartNewTree(key,value);
    return true;
  }
  bool suc=InsertIntoLeaf(key,value,transaction);
  return res;
}
/*
 * Insert constant key & value pair into an empty tree
 * User needs to first ask for new page from buffer pool manager(NOTICE: throw
 * an "out of memory" exception if returned value is nullptr), then update b+
 * tree's root page id and insert entry directly into leaf page.
 */
INDEX_TEMPLATE_ARGUMENTS
void BPLUSTREE_TYPE::StartNewTree(const KeyType &key, const ValueType &value) {
  page_id_t new_page_id;
  Page *rootPage=buffer_pool_manager_->NewPage(new_page_id);
  B_PLUS_TREE_LEAF_PAGE_TYPE *root=reinterpret_cast<B_PLUS_TREE_LEAF_PAGE_TYPE*>(rootPage->GetData());

  root->Init(new_page_id,INVALID_PAGE_ID);
  root_page_id_=new_page_id;
  UpdateRootPageId(1);
  root->Insert(key,value,comparator_);
  buffer_pool_manager_->UnpinPage(new_page_id,true);
}

/*
 * Insert constant key & value pair into leaf page
 * User needs to first find the right leaf page as insertion target, then look
 * through leaf page to see whether insert key exist or not. If exist, return
 * immediately, otherwise insert entry. Remember to deal with split if necessary.
 * @return: since we only support unique key, if user try to insert duplicate
 * keys return false, otherwise return true.
 */
INDEX_TEMPLATE_ARGUMENTS
bool BPLUSTREE_TYPE::InsertIntoLeaf(const KeyType &key, const ValueType &value, Transaction *transaction) {
  B_PLUS_TREE_LEAF_PAGE_TYPE *leaf=FindLeafPage(key,false);
  ValueType v;
  bool exist_in_parent=leaf->Lookup(key,v,comparator_);
  if(exist_in_parent) {
    buffer_pool_manager_->UnpinPage(leaf->GetPageId(),false);
    return false;
  }
  leaf->Insert(key,value,comparator_);
  if(leaf->GetSize()>leaf->GetMaxSize()){
    B_PLUS_TREE_LEAF_PAGE_TYPE *newleaf=Split(leaf);
    InsertIntoParent(leaf,newleaf->KeyAt(0),newleaf,transaction);
  }
  buffer_pool_manager_->UnpinPage(leaf->GetPageId(),true);
  return true;
}

/*
 * Split input page and return newly created page.
 * Using template N to represent either internal page or leaf page.
 * User needs to first ask for new page from buffer pool manager(NOTICE: throw
 * an "out of memory" exception if returned value is nullptr), then move half
 * of key & value pairs from input page to newly created page
 */
INDEX_TEMPLATE_ARGUMENTS
template<typename N>
N *BPLUSTREE_TYPE::Split(N *node) {
  page_id_t newpageID;
  Page* const newPage=buffer_pool_manager_->NewPage(newpageID);
  newPage->WLatch();
  N* newNode=reinterpret_cast<N*>(newPage->GetData());
  newNode->Init(newpageID,node->GetParentPageId());
  node->MoveHalfTo(newNode);
  return newNode;
}

/*
 * Insert key & value pair into internal page after split
 * @param   old_node      input page from split() method
 * @param   key
 * @param   new_node      returned page from split() method
 * User needs to first find the parent page of old_node, parent node must be
 * adjusted to take info of new_node into account. Remember to deal with split
 * recursively if necessary.
 */
INDEX_TEMPLATE_ARGUMENTS
void BPLUSTREE_TYPE::InsertIntoParent(BPlusTreePage *old_node, const KeyType &key, BPlusTreePage *new_node,Transaction *transaction) {
    if(old_node->IsRootPage()) {
      Page * newPage=buffer_pool_manager_->NewPage(root_page_id_);
      B_PLUS_TREE_INTERNAL_PAGE_TYPE *newRoot=reinterpret_cast<B_PLUS_TREE_INTERNAL_PAGE_TYPE *>(newPage->GetData());
      newRoot->Init(root_page_id_);
      newRoot->PopulateNewRoot(old_node->GetPageId(),key,new_node->GetPageId())
      old_node->SetParentPageId(root_page_id_);
      new_node->SetParentPageId(root_page_id_);
      UpdateRootPageId();
      buffer_pool_manager_->UnpinPage(newRoot->GetPageId(),true);
      return;
    }
    page_id_t parentID=old_node->GetParentPageId();
    B_PLUS_TREE_INTERNAL_PAGE_TYPE *parent=reinterpret_cast<B_PLUS_TREE_INTERNAL_PAGE_TYPE *>(buffer_pool_manager_->FetchPage(parentID)->GetData());
    new_node->SetParentPageId(parentID);
    parent->InsertNodeAfter(old_node->GetPageId(),key,new_node->GetPageId());
    if(parent->GetSize()>parent->GetMaxSize()) {
      B_PLUS_TREE_INTERNAL_PAGE_TYPE *newleaf
      InsertIntoParent(parent,new_page->KeyAt(0),new_page,transaction);
    }
    buffer_pool_manager_->UnpinPage(parentID,true);
}

/*****************************************************************************
 * REMOVE
 *****************************************************************************/
/*
 * Delete key & value pair associated with input key
 * If current tree is empty, return immediately.
 * If not, User needs to first find the right leaf page as deletion target, then
 * delete entry from leaf page. Remember to deal with redistribute or merge if
 * necessary.
 */
INDEX_TEMPLATE_ARGUMENTS
void BPLUSTREE_TYPE::Remove(const KeyType &key, Transaction *transaction) {
  if(this->IsEmpty()) return;
  B_PLUS_TREE_LEAF_PAGE_TYPE *leaf_target=FindLeafPage(key,false);
  int size_after_delete=leaf_target->RemoveAndDeleteRecord(key,comparator_);
  if(size_after_delete<leaf_target->GetMinSize()) {
    CoalesceOrRedistribute(leaf_target,transaction);
  }
  buffer_pool_manager_->UnpinPage(leaf_target->GetPageId());
  return;
}

/*
 * User needs to first find the sibling of input page. If sibling's size + input
 * page's size > page's max size, then redistribute. Otherwise, merge.
 * Using template N to represent either internal page or leaf page.
 * @return: true means target leaf page should be deleted, false means no
 * deletion happens
 */
INDEX_TEMPLATE_ARGUMENTS
template<typename N>
bool BPLUSTREE_TYPE::CoalesceOrRedistribute(N *node, Transaction *transaction) {
  if(node->IsRootPage()) {
    bool if_deleted=AdjustRoot(node);
    if(if_deleted) {transaction->AddIntoDeletedPageSet(node->GetPageId())}
    return if_deleted;
  }
  N* brotherPage=FindbrotherPage(node,transaction);
  // BPlusTreePage *parent=buffer_pool_manager_->FetchPage(node->GetParentPageId());
  BPlusTreePage *parent=reinterpret_cast<BPlusTreePage *>(buffer_pool_manager_->FetchPage(node->GetParentPageId())->GetData());
  B_PLUS_TREE_INTERNAL_PAGE_TYPE *parentPage=static_cast<B_PLUS_TREE_INTERNAL_PAGE_TYPE *>(parent);
  int index_in_parent=parentPage->ValueIndex(node->GetPageId());
  if(node->GetSize()+brotherpage->GetSize()<=node->GetMaxSize()){
    // ?????????
    
    Coalesce(&brotherPage,&node,&parentPage,index_in_parent,transaction);
    buffer_pool_manager_->UnpinPage(parentPage->GetPageId(),true);
    return true;
  }
  Redistribute(brotherpage,node,index_in_parent);
  buffer_pool_manager_->UnpinPage(parentPage->GetPageId(),false);
  return false;
}

N* FindbrotherPage(N *node,Transaction *transaction)
{
  // auto * parentPage = buffer_pool_manager_->FetchPage(node->GetParentPageId());
  auto * parentPage =reinterpret_cast<BPlusTreePage *>(buffer_pool_manager_->FetchPage(node->GetParentPageId())->GetData());
  B_PLUS_TREE_INTERNAL_PAGE_TYPE *parent=reinterpret_cast<B_PLUS_TREE_INTERNAL_PAGE_TYPE>(parentPage);
  int index=parent->ValueIndex(node->GetPageId());
  int brother_index;
  if(index==0) brother_index=1;
  else brother_index=index-1;
  //??????????????????????????
  BPlusTreePage *page=buffer_pool_manager->FetchPage(parent->ValueAt(brother_index));
  BPlusTreePage *treepage=reinterpret_cast< BPlusTreePage *>(page->GetData());

  N* brother=reinterpret_cast<N*>(treepage->GetData());
  
  buffer_pool_manager_->UnpinPage(page->GetPageId(),false);
  buffer_pool_manager_->UnpinPage(parent->GetPageId(),false);
  return brother;
}


/*
 * Move all the key & value pairs from one page to its sibling page, and notify
 * buffer pool manager to delete this page. Parent page must be adjusted to
 * take info of deletion into account. Remember to deal with coalesce or
 * redistribute recursively if necessary.
 * Using template N to represent either internal page or leaf page.
 * @param   neighbor_node      sibling page of input "node"
 * @param   node               input from method coalesceOrRedistribute()
 * @param   parent             parent page of input "node"
 * @return  true means parent node should be deleted, false means no deletion happened
 */
INDEX_TEMPLATE_ARGUMENTS
template<typename N>
bool BPLUSTREE_TYPE::Coalesce(N **neighbor_node, N **node,BPlusTreeInternalPage<KeyType, page_id_t, KeyComparator> **parent, int index, Transaction *transaction) {
  (*node)->MoveAllTo(*neighbor_node,index,buffer_pool_manager_);
  transaction->AddIntoDeletedPageSet(node->GetPageId());
  (*parent)->Remove(index);
  if((*parent)->GetSize()<=(*parent)->GetMinSize()) {
    return CoalesceOrRedistribute(*parent,transaction);
  }
  return false;
}

/*
 * Redistribute key & value pairs from one page to its sibling page. If index ==
 * 0, move sibling page's first key & value pair into end of input "node",
 * otherwise move sibling page's last key & value pair into head of input
 * "node".
 * Using template N to represent either internal page or leaf page.
 * @param   neighbor_node      sibling page of input "node"
 * @param   node               input from method coalesceOrRedistribute()
 */
INDEX_TEMPLATE_ARGUMENTS
template<typename N>
void BPLUSTREE_TYPE::Redistribute(N *neighbor_node, N *node, int index) {
  if(index==0) {
    neighbor_node->MoveFirstToEndOf(node,buffer_pool_manager_);
  }else {
    neighbor_node->MoveLastToFrontOf(node,index,buffer_pool_manager_);
  }
}

/*
 * Update root page if necessary
 * NOTE: size of root page can be less than min size and this method is only
 * called within coalesceOrRedistribute() method
 * case 1: when you delete the last element in root page, but root page still
 * has one last child
 * case 2: when you delete the last element in whole b+ tree
 * @return : true means root page should be deleted, false means no deletion
 * happened
 */
INDEX_TEMPLATE_ARGUMENTS
bool BPLUSTREE_TYPE::AdjustRoot(BPlusTreePage *old_root_node) {
  if(old_root_node->IsLeafPage()){
    root_page_id_=INVALID_PAGE_ID;
    UpdateRootPageId();
    return true;
  }
  if(old_root_node->GetSize()==1){
    B_PLUS_TREE_INTERNAL_PAGE_TYPE *root=reinterpret_cast<B_PLUS_TREE_INTERNAL_PAGE_TYPE *>(old_root_node);
    page_id_t newRoot=root->RemoveAndReturnOnlyChild();
    root_page_id_=newRoot;
    UpdateRootPageId();
    Page *page=buffer_pool_manager_->FetchPage(newRoot);
    B_PLUS_TREE_INTERNAL_PAGE_TYPE *newRootPage=reinterpret_cast<B_PLUS_TREE_INTERNAL_PAGE_TYPE*>(page->GetData());
    newRootPage->SetParentPageId(INVALID_PAGE_ID);
    buffer_pool_manager_->UnpinPage(newRoot,true);
    return true;
  }
  return false;
}

/*****************************************************************************
 * INDEX ITERATOR
 *****************************************************************************/
/*
 * Input parameter is void, find the left most leaf page first, then construct
 * index iterator
 * @return : index iterator
 */
INDEX_TEMPLATE_ARGUMENTS
INDEXITERATOR_TYPE BPLUSTREE_TYPE::Begin() {
  KeyType nouse;
  auto start=FindLeafPage(nouse,true);
  return INDEXITERATOR_TYPE(start,0,buffer_pool_manager_);
}

/*
 * Input parameter is low key, find the leaf page that contains the input key
 * first, then construct index iterator
 * @return : index iterator
 */
INDEX_TEMPLATE_ARGUMENTS
INDEXITERATOR_TYPE BPLUSTREE_TYPE::Begin(const KeyType &key) {
  auto start=FindLeafPage(key);
  if(start==nullptr) {
    return INDEXITERATOR_TYPE(start,0,buffer_pool_manager_);
  }
  int index=start->KeyIndex(key,comparator_);
  return INDEXITERATOR_TYPE(start,index,buffer_pool_manager_);
}

/*
 * Input parameter is void, construct an index iterator representing the end
 * of the key/value pair in the leaf node
 * @return : index iterator
 */
INDEX_TEMPLATE_ARGUMENTS
INDEXITERATOR_TYPE BPLUSTREE_TYPE::End() {
  KeyType key{};
  B_PLUS_TREE_LEAF_PAGE_TYPE *leaf= reinterpret_cast<B_PLUS_TREE_LEAF_PAGE_TYPE*>( FindLeafPage(key, true));
  page_id_t new_page;
  while(leaf->GetNextPageId()!=INVALID_PAGE_ID){
    new_page=leaf->GetNextPageId();
    leaf=reinterpret_cast<B_PLUS_TREE_LEAF_PAGE_TYPE*>(buffer_pool_manager_->FetchPage(new_page));
  }
  buffer_pool_manager_->UnpinPage(new_page,false);
  return INDEXITERATOR_TYPE(leaf, leaf->GetSize(), buffer_pool_manager_);
}

/*****************************************************************************
 * UTILITIES AND DEBUG
 *****************************************************************************/
/*
 * Find leaf page containing particular key, if leftMost flag == true, find
 * the left most leaf page
 * Note: the leaf page is pinned, you need to unpin it after use.
 */
INDEX_TEMPLATE_ARGUMENTS
Page *BPLUSTREE_TYPE::FindLeafPage(const KeyType &key, bool leftMost) {
  if(IsEmpty()) return nullptr;
  // auto pointer=buffer_pool_manager_->FetchPage(root_page_id_);
  BPlusTreePage* pointer=reinterpret_cast<BPlusTreePage *>(buffer_pool_manager_->FetchPage(root_page_id_)->GetData());
  page_id_t next;
  for(page_id_t i=root_page_id_;!p->IsLeafPage();i=next,pointer=reinterpret_cast<BPlusTreePage *>(buffer_pool_manager_->FetchPage(i)->GetData());)
  {
    B_PLUS_TREE_INTERNAL_PAGE_TYPE *internalPage=static_cast<B_PLUS_TREE_INTERNAL_PAGE_TYPE *>(pointer);
    if(leftMost){
      next=internalPage->ValueAt(0);
    }
    else {
      next=internalPage->Lookup(key,comparator_);
    }
    buffer_pool_manager_->UnpinPage(i,false);
  }
  return nullptr;
}

/*
 * Update/Insert root page id in header page(where page_id = 0, header_page is
 * defined under include/page/header_page.h)
 * Call this method everytime root page id is changed.
 * @parameter: insert_record      default value is false. When set to true,
 * insert a record <index_name, root_page_id> into header page instead of
 * updating it.
 */
INDEX_TEMPLATE_ARGUMENTS
void BPLUSTREE_TYPE::UpdateRootPageId(int insert_record) {
   IndexRootsPage* root_page_set=static_cast<IndexRootsPage*>(buffer_pool_manager_->FetchPage(HEADER_PAGE_ID));
   if(insert_record){
     root_page_set->Insert(index_id_,root_page_id_);
   }
   else {
     root_page_set->Update(index_id_,root_page_id_);
   }
   buffer_pool_manager_->UnpinPage(HEADER_PAGE_ID,true);
}

/**
 * This method is used for debug only, You don't need to modify
 */
INDEX_TEMPLATE_ARGUMENTS
void BPLUSTREE_TYPE::ToGraph(BPlusTreePage *page, BufferPoolManager *bpm, std::ofstream &out) const {
  std::string leaf_prefix("LEAF_");
  std::string internas("INT_");
  if (page->IsLeafPage()) {
    auto *leaf = reinterpret_cast<LeafPage *>(page);
    // Print node name
    out << leaf_prefix << leaf->GetPageId();
    // Print node properties
    out << "[shape=plain color=green ";
    // Print data of the node
    out << "label=<<TABLE BORDER=\"0\" CELLBORDER=\"1\" CELLSPACING=\"0\" CELLPADDING=\"4\">\n";
    // Print data
    out << "<TR><TD COLSPAN=\"" << leaf->GetSize() << "\">P=" << leaf->GetPageId()
        << ",Parent=" << leaf->GetParentPageId() << "</TD></TR>\n";
    out << "<TR><TD COLSPAN=\"" << leaf->GetSize() << "\">"
        << "max_size=" << leaf->GetMaxSize() << ",min_size=" << leaf->GetMinSize() << ",size=" << leaf->GetSize()
        << "</TD></TR>\n";
    out << "<TR>";
    for (int i = 0; i < leaf->GetSize(); i++) {
      out << "<TD>" << leaf->KeyAt(i) << "</TD>\n";
    }
    out << "</TR>";
    // Print table end
    out << "</TABLE>>];\n";
    // Print Leaf node link if there is a next page
    if (leaf->GetNextPageId() != INVALID_PAGE_ID) {
      out << leaf_prefix << leaf->GetPageId() << " -> " << leaf_prefix << leaf->GetNextPageId() << ";\n";
      out << "{rank=same " << leaf_prefix << leaf->GetPageId() << " " << leaf_prefix << leaf->GetNextPageId()
          << "};\n";
    }

    // Print parent links if there is a parent
    if (leaf->GetParentPageId() != INVALID_PAGE_ID) {
      out << internal_prefix << leaf->GetParentPageId() << ":p" << leaf->GetPageId() << " -> " << leaf_prefix
          << leaf->GetPageId() << ";\n";
    }
  } else {
    auto *inner = reinterpret_cast<InternalPage *>(page);
    // Print node name
    out << internal_prefix << inner->GetPageId();
    // Print node properties
    out << "[shape=plain color=pink ";  // why not?
    // Print data of the node
    out << "label=<<TABLE BORDER=\"0\" CELLBORDER=\"1\" CELLSPACING=\"0\" CELLPADDING=\"4\">\n";
    // Print data
    out << "<TR><TD COLSPAN=\"" << inner->GetSize() << "\">P=" << inner->GetPageId()
        << ",Parent=" << inner->GetParentPageId() << "</TD></TR>\n";
    out << "<TR><TD COLSPAN=\"" << inner->GetSize() << "\">"
        << "max_size=" << inner->GetMaxSize() << ",min_size=" << inner->GetMinSize() << ",size=" << inner->GetSize()
        << "</TD></TR>\n";
    out << "<TR>";
    for (int i = 0; i < inner->GetSize(); i++) {
      out << "<TD PORT=\"p" << inner->ValueAt(i) << "\">";
      if (i > 0) {
        out << inner->KeyAt(i);
      } else {
        out << " ";
      }
      out << "</TD>\n";
    }
    out << "</TR>";
    // Print table end
    out << "</TABLE>>];\n";
    // Print Parent link
    if (inner->GetParentPageId() != INVALID_PAGE_ID) {
      out << internal_prefix << inner->GetParentPageId() << ":p" << inner->GetPageId() << " -> "
          << internal_prefix
          << inner->GetPageId() << ";\n";
    }
    // Print leaves
    for (int i = 0; i < inner->GetSize(); i++) {
      auto child_page = reinterpret_cast<BPlusTreePage *>(bpm->FetchPage(inner->ValueAt(i))->GetData());
      ToGraph(child_page, bpm, out);
      if (i > 0) {
        auto sibling_page = reinterpret_cast<BPlusTreePage *>(bpm->FetchPage(inner->ValueAt(i - 1))->GetData());
        if (!sibling_page->IsLeafPage() && !child_page->IsLeafPage()) {
          out << "{rank=same " << internal_prefix << sibling_page->GetPageId() << " " << internal_prefix
              << child_page->GetPageId() << "};\n";
        }
        bpm->UnpinPage(sibling_page->GetPageId(), false);
      }
    }
  }
  bpm->UnpinPage(page->GetPageId(), false);
}

/**
 * This function is for debug only, you don't need to modify
 */
INDEX_TEMPLATE_ARGUMENTS
void BPLUSTREE_TYPE::ToString(BPlusTreePage *page, BufferPoolManager *bpm) const {
  if (page->IsLeafPage()) {
    auto *leaf = reinterpret_cast<LeafPage *>(page);
    std::cout << "Leaf Page: " << leaf->GetPageId() << " parent: " << leaf->GetParentPageId()
              << " next: " << leaf->GetNextPageId() << std::endl;
    for (int i = 0; i < leaf->GetSize(); i++) {
      std::cout << leaf->KeyAt(i) << ",";
    }
    std::cout << std::endl;
    std::cout << std::endl;
  } else {
    auto *internal = reinterpret_cast<InternalPage *>(page);
    std::cout << "Internal Page: " << internal->GetPageId() << " parent: " << internal->GetParentPageId()
              << std::endl;
    for (int i = 0; i < internal->GetSize(); i++) {
      std::cout << internal->KeyAt(i) << ": " << internal->ValueAt(i) << ",";
    }
    std::cout << std::endl;
    std::cout << std::endl;
    for (int i = 0; i < internal->GetSize(); i++) {
      ToString(reinterpret_cast<BPlusTreePage *>(bpm->FetchPage(internal->ValueAt(i))->GetData()), bpm);
      bpm->UnpinPage(internal->ValueAt(i), false);
    }
  }
}

INDEX_TEMPLATE_ARGUMENTS
bool BPLUSTREE_TYPE::Check() {
  bool all_unpinned = buffer_pool_manager_->CheckAllUnpinned();
  if (!all_unpinned) {
    LOG(ERROR) << "problem in page unpin" << endl;
  }
  return all_unpinned;
}

template
class BPlusTree<int, int, BasicComparator<int>>;

template
class BPlusTree<GenericKey<4>, RowId, GenericComparator<4>>;

template
class BPlusTree<GenericKey<8>, RowId, GenericComparator<8>>;

template
class BPlusTree<GenericKey<16>, RowId, GenericComparator<16>>;

template
class BPlusTree<GenericKey<32>, RowId, GenericComparator<32>>;

template
class BPlusTree<GenericKey<64>, RowId, GenericComparator<64>>;
