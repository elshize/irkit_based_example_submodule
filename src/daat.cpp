#include <iostream>
#include <chrono>
#include <string>
#include <sstream>

#include <irkit/coding/varbyte.hpp>
#include <irkit/index.hpp>
#include <irkit/io.hpp>
#include <irkit/memoryview.hpp>
#include <irkit/index/source.hpp>
#include <irkit/index/types.hpp>
#include <irkit/index/posting_list.hpp>

using irk::index::document_t;
using doc_list_t = irk::index::block_document_list_view;
using payload_list_t = irk::index::block_payload_list_view<long>;
using post_list_t = irk::posting_list_view<doc_list_t, payload_list_t>;

template<class Document, class Score>
struct Posting {
    Document document;
    Score score;
};

bool order(const Posting<long, long>& lhs, 
            const Posting<long, long>& rhs)
{
    return lhs.score > rhs.score;
}

auto daat(int k, const std::vector<post_list_t>& query,
          long collection_size){
    std::vector<post_list_t::iterator> pointers;
    auto cur_posting = query[0].begin().document();
    std::vector<Posting<long, long> > topk;
    long threshold = 0;
    for(auto plist : query){
        pointers.push_back(plist.begin());
        if(plist.begin().document() < cur_posting){
            cur_posting = plist.begin().document();
        }
    }
    int flag;
    long cur_score = 0;
    while(flag < pointers.size()){
        flag = 0;
        cur_score = 0;
        document_t min = document_t(collection_size);
        for(size_t i = 0; i < pointers.size(); ++i){
            if(pointers[i] != query[i].end()){
                auto next = pointers[i].nextgeq(cur_posting);
                if(next.document() == cur_posting){
                    cur_score += next.payload();
                    next.moveto(cur_posting + 1);
                    min = std::min(min, next.document());
                }
                else{
                    min = std::min(min, pointers[i].document());
                }
            }
            else{
                ++flag;
            }
        }
        if(cur_score > threshold){
            Posting<long, long> p {long(cur_posting), cur_score};
            topk.push_back(p);
            if(topk.size() <= k){
                std::push_heap(topk.begin(), topk.end(), order);
            }
            else{
                std::pop_heap(topk.begin(), topk.end(), order);
                topk.pop_back();
            }
            threshold = topk.size() == k ? topk[0].score : threshold;
        }
        cur_posting = min;
    }
    std::sort(topk.begin(), topk.end(), order);
    return topk;
}

int main(){

}