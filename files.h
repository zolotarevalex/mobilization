#ifndef FILES_H_
#define FILES_H_

const char* make_file_name(char* buf, int size, const char* base, int base_size, const char* ext, int ext_size);
const char* make_in_name(char* buf, int size, const char* base, int base_size);
const char* make_out_name(char* buf, int size, const char* base, int base_size);

void gen_test_files(int files_num, int file_size, const char* base_name);
void gen_file(int file_size, const char* name);

const char* read_test_file(const char* name);
void save_test_file(const char* buf, int size, const char* name);

#endif //FILES_H_