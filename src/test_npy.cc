#include "npy_head.h"
#include <string>
#include <iostream>
int main(){
    std::string file_path = "./test.npy"; 
    
    std::cout <<"begin to test normal load"<<std::endl;
    gpex::common::numpy::npy_file np_file = gpex::common::numpy::load_npy_data(file_path);
    double* res = (double*) np_file.ptr();
    for(int index = 0; index < gpex::common::numpy::compute_size(np_file.m_shape); index++){
        std::cout<<res[index]<<" ";
    }
    std::cout <<std::endl<<"begin to test foreign allocated buffer"<<std::endl;
        
    gpex::common::numpy::npy_file np_file_meta = gpex::common::numpy::load_npy_meta(file_path);
    size_t array_size = gpex::common::numpy::compute_size(np_file_meta.m_shape);
    double* res2 = (double*) malloc(array_size * sizeof(double));
    gpex::common::numpy::npy_file np_file_2 = gpex::common::numpy::load_npy_data(file_path, false, res2);
    for(int index = 0; index < gpex::common::numpy::compute_size(np_file_meta.m_shape); index++){
        std::cout<<res2[index]<<" ";
    }
    
    std::cout <<std::endl<<"begin to test memory map mode"<<std::endl;
    gpex::common::numpy::npy_file np_file_3 = gpex::common::numpy::load_npy_data(file_path, true);
    double* res3 = (double*) (&(np_file_3.ptr()[np_file_3.array_offset]));  
    for(int index = 0; index < gpex::common::numpy::compute_size(np_file_3.m_shape); index++){
        std::cout<<res3[index]<<" ";
    }
}