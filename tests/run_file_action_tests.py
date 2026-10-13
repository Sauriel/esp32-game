#!/usr/bin/env python3
"""Test real FileActions against a temporary host filesystem, not ESP firmware."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix='esp32-file-actions-') as temporary:
    work = Path(temporary)
    (work/'esp_log.h').write_text('''#pragma once
#include <stdio.h>
#define ESP_LOGI(tag, format, ...) ((void)printf(format "\\n", ##__VA_ARGS__))
#define ESP_LOGW(tag, format, ...) ((void)printf(format "\\n", ##__VA_ARGS__))
''')
    (work/'ff.h').write_text('''#pragma once
#include <stdint.h>
typedef uint32_t DWORD;
DWORD ff_wtoupper(DWORD);
''')
    (work/'test.cpp').write_text(r'''
#include "transfer/FileActions.h"
#include "storage/SdStorage.h"
#include <filesystem>
#include <fstream>
#include <string>
#include <cassert>
#include <cerrno>
#include <cstring>
#include <dirent.h>
#include <unistd.h>
#include <cctype>
namespace fs=std::filesystem;
fs::path root;
DIR* iterator=nullptr;
bool failRename=false;
extern "C" uint32_t ff_wtoupper(uint32_t c){
 if(c>='a'&&c<='z')return c-'a'+'A';
 if(c==0xe4)return 0xc4;
 return c;
}
// Mimic FAT case-insensitive ASCII lookup; real directory operations occur on disk.
std::string folded(std::string s){for(char&c:s)if(c>='a'&&c<='z')c-=32;return s;}
fs::path resolve(const char* path){
 fs::path current=root;
 for(const auto& part:fs::path(path).relative_path()){
  if(!fs::is_directory(current)){errno=ENOTDIR;return {};}
  bool found=false;
  for(const auto& entry:fs::directory_iterator(current)){
   if(folded(entry.path().filename().string())==folded(part.string())){current=entry.path();found=true;break;}
  }
  if(!found){errno=ENOENT;return {};}
 }
 return current;
}
namespace SdStorage {
bool ready(){return true;}
bool statPath(const char* p,struct stat& info){const auto full=resolve(p);return !full.empty()&&::stat(full.c_str(),&info)==0;}
bool makeDirectory(const char*p){return ::mkdir((root/fs::path(p).relative_path()).c_str(),0777)==0;}
bool removeDirectory(const char*p){return ::rmdir((root/fs::path(p).relative_path()).c_str())==0;}
bool remove(const char*p){return ::unlink((root/fs::path(p).relative_path()).c_str())==0;}
bool rename(const char*a,const char*b){
 if(failRename){errno=EIO;return false;}
 if(!resolve(b).empty()){errno=EEXIST;return false;}
 return ::rename((root/fs::path(a).relative_path()).c_str(),(root/fs::path(b).relative_path()).c_str())==0;
}
bool listBegin(const char*p){assert(!iterator);const auto full=resolve(p);if(full.empty())return false;iterator=::opendir(full.c_str());return iterator;}
int listNext(char*n,size_t size){errno=0;const auto entry=::readdir(iterator);if(!entry)return errno?-1:0;if(strlen(entry->d_name)>=size){errno=ENAMETOOLONG;return -1;}strcpy(n,entry->d_name);return 1;}
bool listEnd(){const bool ok=::closedir(iterator)==0;iterator=nullptr;return ok;}
}
using FileActions::Action;using FileActions::Error;
void expect(Action action,const char* source,const char* target,Error expected){assert(FileActions::execute(action,source,target).error==expected);assert(!iterator);}
void writeFile(const fs::path& path){std::ofstream(path)<<"preserved asset bytes";}
void preserved(const fs::path& path){std::ifstream in(path);std::string bytes((std::istreambuf_iterator<char>(in)),{});assert(bytes=="preserved asset bytes");}
int main(int argc,char** argv){
 assert(argc==2);root=argv[1];fs::create_directories(root);
 expect(Action::CreateDirectory,"/Assets",nullptr,Error::None);
 expect(Action::CreateDirectory,"/assets",nullptr,Error::Conflict);
 expect(Action::CreateDirectory,"/missing/new",nullptr,Error::NotFound);
 expect(Action::Delete,"/",nullptr,Error::Invalid);
 fs::create_directories(root/"Assets/Chest/sub");writeFile(root/"Assets/Chest/item.bin");
 expect(Action::Delete,"/Assets/Chest",nullptr,Error::Conflict);preserved(root/"Assets/Chest/item.bin");
 expect(Action::Move,"/assets/chest","/ASSETS/CHEST/sub/moved",Error::Invalid);
 preserved(root/"Assets/Chest/item.bin");
 expect(Action::Rename,"/Assets/Chest","/elsewhere/new",Error::NotFound);
 expect(Action::Rename,"/Assets/Chest","/Assets/Box",Error::None);preserved(root/"Assets/Box/item.bin");
 expect(Action::Rename,"/Assets/Box","/Assets/box",Error::Conflict);
 expect(Action::Rename,"/Assets/Box","/OtherName",Error::Invalid);
 expect(Action::Move,"/Assets/Box","/Box",Error::None);preserved(root/"Box/item.bin");
 fs::create_directories(root/"Box2");
 expect(Action::Move,"/Box","/Box2/Box",Error::None);preserved(root/"Box2/Box/item.bin");
 writeFile(root/"existing.bin");
 expect(Action::Move,"/Box2/Box/item.bin","/existing.bin",Error::Conflict);preserved(root/"existing.bin");
 expect(Action::Rename,"/Box2/Box/item.bin","/Box2/Box/hero.bin",Error::None);preserved(root/"Box2/Box/hero.bin");
 failRename=true;expect(Action::Move,"/Box2/Box/hero.bin","/Assets/hero.bin",Error::Storage);failRename=false;
 preserved(root/"Box2/Box/hero.bin");
 expect(Action::Delete,"/Box2/Box/hero.bin",nullptr,Error::None);assert(!fs::exists(root/"Box2/Box/hero.bin"));
 expect(Action::Delete,"/Box2/Box/sub",nullptr,Error::None);
 expect(Action::Delete,"/Box2/Box",nullptr,Error::None);
 expect(Action::Delete,"/absent.bin",nullptr,Error::NotFound);
 fs::create_directories(root/u8"Ä/sub");
 expect(Action::Move,u8"/ä",u8"/Ä/sub/new",Error::Invalid);
 fs::create_directories(root/"LongDirectory/sub");
 expect(Action::Move,"/LongDirectory","/LONGDI~1/sub/new",Error::NotFound);
 puts("PASS file actions: create, delete, rename, move, conflicts, root, self/descendant, Unicode, aliases and I/O failure");
}
''')
    executable=work/'test'
    subprocess.run(['g++','-std=c++17','-Wall','-Wextra','-Werror',
                    '-I'+str(work),'-I'+str(root/'main'),str(work/'test.cpp'),
                    str(root/'main/transfer/FileActions.cpp'),'-o',str(executable)],check=True)
    subprocess.run([str(executable),str(work/'card')],check=True)
