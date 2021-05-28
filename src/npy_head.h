#ifndef GPEX_COMMON_ARRAY_ARRAY_NPY_H_
#define GPEX_COMMON_ARRAYARRAY__NPY_H_
#include <cstdint>
#include <cstring>
#include <memory>
#include <regex>
#include <stdexcept>
#include <string>
#include <vector>
#include <stdio.h>
#include <unistd.h>
#include <sys/mman.h>
#include <unistd.h>
#include <sstream>
#include <iostream>

namespace gpex{
    namespace common{
        namespace numpy{
            const char magic_string[] = "\x93NUMPY";
            const std::size_t magic_string_length = sizeof(magic_string) - 1;

            inline void read_magic(FILE *fp, unsigned char* v_major,unsigned char* v_minor)
            {
                std::unique_ptr<char[]> buf(new char[magic_string_length + 2]);

                if (fread(buf.get(),1, magic_string_length + 2, fp) == NULL)
                {
                    throw "io error: failed reading file" ;
                }

                for (std::size_t i = 0; i < magic_string_length; i++)
                {
                    if (buf[i] != magic_string[i])
                    {
                        throw "this file do not have a valid npy format.";
                    }
                }

                *v_major = static_cast<unsigned char>(buf[magic_string_length]);
                *v_minor = static_cast<unsigned char>(buf[magic_string_length + 1]);
            }
            size_t compute_size(std::vector<size_t> shape){
                size_t size = 1;
                for(int index = 0; index < shape.size(); index++){
                    size *= shape[index];
                }
                return size;
            }
            struct npy_file
            {
                npy_file() = default;

                npy_file(std::vector<std::size_t>& shape, bool fortran_order, std::string typestring)
                    : m_shape(shape), m_fortran_order(fortran_order), m_typestring(typestring)
                {
                    // Allocate memory
                    m_word_size = std::size_t(atoi(&typestring[2]));
                    m_n_bytes = compute_size(shape) * m_word_size;
                    m_buffer = nullptr;
                    
                }

                ~npy_file()
                {
                    if(memmap_mode && m_buffer){
                        munmap(m_buffer, m_n_bytes);
                    }else if (!memmap_mode && m_buffer){
                        delete m_buffer;
                    }
                    
                }
                

                // delete copy constructor
                npy_file(const npy_file&) = delete;
                npy_file& operator=(npy_file) = delete;

                // implement move constructor and assignment
                npy_file(npy_file&& rhs)
                    : m_shape(std::move(rhs.m_shape)),
                    m_fortran_order(std::move(rhs.m_fortran_order)),
                    m_word_size(std::move(rhs.m_word_size)),
                    m_n_bytes(std::move(rhs.m_n_bytes)),
                    m_typestring(std::move(rhs.m_typestring)),
                    m_buffer(rhs.m_buffer),
                    memmap_mode(rhs.memmap_mode),
                    array_offset(rhs.array_offset)
                {
                    
                }

                void load(FILE* fp, bool mmap_mode = false){
                    if(!mmap_mode){
                        memmap_mode = false;
                        m_buffer = new char[m_n_bytes];
                        size_t read_size = fread(m_buffer, 1, m_n_bytes, fp);
                        if(read_size != m_n_bytes){
                            throw "read array failed";
                        }
                    }else{
                        int fd = fileno(fp);
                        memmap_mode = true;
                        array_offset = ftell(fp);
                        m_buffer = (char*) mmap(NULL, m_n_bytes, PROT_READ, MAP_SHARED, fd, 0);
                        
                        
                    }
                }
                void load(FILE* fp, void* allocated_buffer){
                    memmap_mode = false;
                    m_buffer = nullptr;
                    size_t read_size = fread(allocated_buffer, 1, m_n_bytes, fp);
                    if(read_size != m_n_bytes){
                        throw "read array failed";
                    }

                }
                npy_file& operator=(npy_file&& rhs)
                {
                    if (this != &rhs)
                    {
                        m_shape = std::move(rhs.m_shape);
                        m_fortran_order = std::move(rhs.m_fortran_order);
                        m_word_size = std::move(rhs.m_word_size);
                        m_n_bytes = std::move(rhs.m_n_bytes);
                        m_typestring = std::move(rhs.m_typestring);
                        m_buffer = rhs.m_buffer;
                        memmap_mode = rhs.memmap_mode;
                        array_offset = rhs.array_offset;
                        rhs.m_buffer = nullptr;
                        
                    }
                    return *this;
                }
                char* ptr()
                {
                    return m_buffer;
                }

                std::size_t n_bytes()
                {
                    return m_n_bytes;
                }

                std::vector<std::size_t> m_shape;
                bool m_fortran_order;
                bool memmap_mode;
                size_t array_offset;
                std::size_t m_word_size;
                std::size_t m_n_bytes;
                std::string m_typestring;
                char* m_buffer;
                
            };
            inline std::string read_header_1_0(FILE* fp){
                // read header length and convert from little endian
                char header_len_le16[2];
                fread(header_len_le16, 1, 2, fp);

                uint16_t header_length = uint16_t(header_len_le16[0] << 0) | uint16_t(header_len_le16[1] << 8);

                if ((magic_string_length + 2 + 2 + header_length) % 16 != 0)
                {

                    throw "header read error";
                }

                std::unique_ptr<char[]> buf(new char[header_length]);
                fread(buf.get(),1, header_length, fp);
                std::string header(buf.get(), header_length);
                return header;
            }

            inline std::string read_header_2_0(FILE* fp)
            {
                // read header length and convert from little endian
                char header_len_le32[4];
                fgets(header_len_le32, 4, fp);

                uint32_t header_length = uint32_t(header_len_le32[0] << 0) | uint32_t(header_len_le32[1] << 8) |
                    uint32_t(header_len_le32[2] << 16) | uint32_t(header_len_le32[3] << 24);

                if ((magic_string_length + 2 + 4 + header_length) % 16 != 0)
                {
                    throw "header read failed";
                }

                std::unique_ptr<char[]> buf(new char[header_length]);
                fread(buf.get(),1, header_length, fp);
                std::string header(buf.get(), header_length);

                return header;
            }
            // Helpers for the improvised parser
            inline std::string unwrap_s(std::string s, char delim_front, char delim_back)
            {
                if ((s.back() == delim_back) && (s.front() == delim_front))
                {
                    return s.substr(1, s.length() - 2);
                }
                else
                {
                    throw "unable to unwrap" ;
                }
            }
            inline void pop_char(std::string& s, char c)
            {
                if (s.back() == c)
                {
                    s.pop_back();
                }
            }
            inline std::string get_value_from_map(std::string mapstr)
            {
                std::size_t sep_pos = mapstr.find_first_of(":");
                if (sep_pos == std::string::npos)
                {
                    return "";
                }

                return mapstr.substr(sep_pos + 1);
            }
            // Safety check function
            inline void parse_typestring(std::string typestring)
            {
                std::regex re("'([<>|])([ifucb])(\\d+)'");
                std::smatch sm;

                std::regex_match(typestring, sm, re);
                if (sm.size() != 4)
                {
                    throw "invalid typestring";
                }
            }
            inline void parse_header(std::string header, std::string& descr,
                                 bool* fortran_order,
                                 std::vector<std::size_t>& shape)
            {
                // The first 6 bytes are a magic string: exactly "x93NUMPY".
                //
                // The next 1 byte is an unsigned byte: the major version number of the file
                // format, e.g. x01.
                //
                // The next 1 byte is an unsigned byte: the minor version number of the file
                // format, e.g. x00. Note: the version of the file format is not tied to the
                // version of the numpy package.
                //
                // The next 2 bytes form a little-endian unsigned short int: the length of the
                // header data HEADER_LEN.
                //
                // The next HEADER_LEN bytes form the header data describing the array's
                // format. It is an ASCII string which contains a Python literal expression of
                // a dictionary. It is terminated by a newline ('n') and padded with spaces
                // ('x20') to make the total length of the magic string + 4 + HEADER_LEN be
                // evenly divisible by 16 for alignment purposes.
                //
                // The dictionary contains three keys:
                //
                // "descr" : dtype.descr
                // An object that can be passed as an argument to the numpy.dtype()
                // constructor to create the array's dtype.
                // "fortran_order" : bool
                // Whether the array data is Fortran-contiguous or not. Since
                // Fortran-contiguous arrays are a common form of non-C-contiguity, we allow
                // them to be written directly to disk for efficiency.
                // "shape" : tuple of int
                // The shape of the array.
                // For repeatability and readability, this dictionary is formatted using
                // pprint.pformat() so the keys are in alphabetic order.

                // remove trailing newline
                if (header.back() != '\n')
                {
                    throw "invalid header";
                }
                header.pop_back();

                // remove all whitespaces
                header.erase(std::remove(header.begin(), header.end(), ' '), header.end());

                // unwrap dictionary
                header = unwrap_s(header, '{', '}');

                // find the positions of the 3 dictionary keys
                std::size_t keypos_descr = header.find("'descr'");
                std::size_t keypos_fortran = header.find("'fortran_order'");
                std::size_t keypos_shape = header.find("'shape'");

                // make sure all the keys are present
                if (keypos_descr == std::string::npos)
                {
                    throw "missing 'descr' key";
                }
                if (keypos_fortran == std::string::npos)
                {
                    throw "missing 'fortran_order' key";
                }
                if (keypos_shape == std::string::npos)
                {
                    throw "missing 'shape' key";
                }

                // Make sure the keys are in order.
                // Note that this violates the standard, which states that readers *must* not
                // depend on the correct order here.
                // TODO: fix
                if (keypos_descr >= keypos_fortran || keypos_fortran >= keypos_shape)
                {
                    throw "header keys in wrong order";
                }

                // get the 3 key-value pairs
                std::string keyvalue_descr;
                keyvalue_descr = header.substr(keypos_descr, keypos_fortran - keypos_descr);
                pop_char(keyvalue_descr, ',');

                std::string keyvalue_fortran;
                keyvalue_fortran = header.substr(keypos_fortran, keypos_shape - keypos_fortran);
                pop_char(keyvalue_fortran, ',');

                std::string keyvalue_shape;
                keyvalue_shape = header.substr(keypos_shape, std::string::npos);
                pop_char(keyvalue_shape, ',');

                // get the values (right side of `:')
                std::string descr_s = get_value_from_map(keyvalue_descr);
                std::string fortran_s = get_value_from_map(keyvalue_fortran);
                std::string shape_s = get_value_from_map(keyvalue_shape);

                parse_typestring(descr_s);
                descr = unwrap_s(descr_s, '\'', '\'');

                // convert literal Python bool to C++ bool
                if (fortran_s == "True")
                {
                    *fortran_order = true;
                }
                else if (fortran_s == "False")
                {
                    *fortran_order = false;
                }
                else
                {
                    throw "invalid fortran_order value";
                }

                // parse the shape Python tuple ( x, y, z,)

                // first clear the vector
                shape.clear();
                shape_s = unwrap_s(shape_s, '(', ')');

                // a tokenizer would be nice...
                std::size_t pos = 0;
                for (;;)
                {
                    std::size_t pos_next = shape_s.find_first_of(',', pos);
                    std::string dim_s;

                    if (pos_next != std::string::npos)
                    {
                        dim_s = shape_s.substr(pos, pos_next - pos);
                    }
                    else
                    {
                        dim_s = shape_s.substr(pos);
                    }

                    if (dim_s.length() == 0)
                    {
                        if (pos_next != std::string::npos)
                        {
                            throw "invalid shape";
                        }
                    }
                    else
                    {
                        std::stringstream ss;
                        ss << dim_s;
                        std::size_t tmp;
                        ss >> tmp;
                        shape.push_back(tmp);
                    }

                    if (pos_next != std::string::npos)
                    {
                        pos = ++pos_next;
                    }
                    else
                    {
                        break;
                    }
                }
            }



            inline npy_file load_npy_data(std::string file_path, bool memmap_mode=false, void* allocated_buffer=NULL)
            {
                // check magic bytes an version number
                FILE* fp = fopen(file_path.c_str(), "rb");
                if(fp == NULL){
                    throw "file open failed";
                }
                unsigned char v_major, v_minor;
                read_magic(fp, &v_major, &v_minor);

                std::string header;

                if (v_major == 1 && v_minor == 0)
                {
                    header = read_header_1_0(fp);
                }
                else if (v_major == 2 && v_minor == 0)
                {
                    header = read_header_2_0(fp);
                }
                else
                {
                    throw "unsupported file format version";
                }
                // parse header
                bool fortran_order;
                std::string typestr;

                std::vector<std::size_t> shape;
                parse_header(header, typestr, &fortran_order, shape);

                npy_file result(shape, fortran_order, typestr);
                // read the data
                if(allocated_buffer != NULL){
                    result.load(fp, allocated_buffer);
                }else{
                    result.load(fp, memmap_mode);
                }
                return result;
            }

            inline npy_file load_npy_meta(std::string file_path)
            {
                // check magic bytes an version number
                FILE* fp = fopen(file_path.c_str(), "rb");
                if(fp == NULL){
                    throw "file open failed";
                }
                unsigned char v_major, v_minor;
                read_magic(fp, &v_major, &v_minor);

                std::string header;

                if (v_major == 1 && v_minor == 0)
                {
                    header = read_header_1_0(fp);
                }
                else if (v_major == 2 && v_minor == 0)
                {
                    header = read_header_2_0(fp);
                }
                else
                {
                    throw "unsupported file format version";
                }
                // parse header
                bool fortran_order;
                std::string typestr;

                std::vector<std::size_t> shape;
                parse_header(header, typestr, &fortran_order, shape);

                npy_file result(shape, fortran_order, typestr);
                fclose(fp);
                return result;
            }


        }
    }
}
#endif