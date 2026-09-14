#include "extension_data.hpp"
#include <iostream>

#ifdef _WIN32
int wmain(int argc,wchar_t** argv) {
#else
int main(int argc,char** argv) {
#endif
    try {
        if(argc!=2) throw std::runtime_error("Usage: cssx_validate <extension directory>");
#ifdef _WIN32
        const auto root=css::fs::path(argv[1]);
#else
        const auto root=css::extensions::utf8_path(argv[1]);
#endif
        const auto file=css::extensions::contained_file(root,"extension.json");
        if(css::fs::file_size(file)>65536) throw std::runtime_error("Manifest exceeds 64 KiB");
        const auto manifest=css::extensions::Manifest::parse(css::read_json(file),root);
        if(!manifest.menu.empty()) {
            if(css::fs::file_size(manifest.menu)>1024*1024) throw std::runtime_error("Menu exceeds 1 MiB");
            (void)css::extensions::bind_menu(css::read_json(manifest.menu),css::Json::object());
        }
        std::cout<<manifest.json().dump(2)<<'\n';
        return 0;
    } catch(const std::exception& error) {
        std::cerr<<error.what()<<'\n';
        return 1;
    }
}
