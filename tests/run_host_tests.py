#!/usr/bin/env python3
"""Isolierte C++-Hosttests; kein PlatformIO-/Firmware-Build, keine Downloads."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix='esp32-transfer-tests-') as temporary:
    work = Path(temporary)
    headers = {
        'esp_system.h': '''#pragma once
#include <stdint.h>
using esp_err_t=int;
constexpr int ESP_OK=0, ESP_FAIL=-1, ESP_ERR_NVS_NOT_FOUND=1, ESP_ERR_NVS_TYPE_MISMATCH=2;
constexpr int ESP_RST_SW=3, ESP_RST_POWERON=1, ESP_RST_EXT=2;
int esp_reset_reason(); unsigned esp_get_minimum_free_heap_size(); unsigned esp_get_free_heap_size(); const char* esp_err_to_name(int);
''',
        'nvs_flash.h': '#pragma once\n#include "esp_system.h"\nint nvs_flash_init();\n',
        'nvs.h': '''#pragma once
#include "esp_system.h"
using nvs_handle_t=unsigned;
constexpr int NVS_READWRITE=1;
int nvs_open(const char*,int,nvs_handle_t*); int nvs_get_u32(nvs_handle_t,const char*,uint32_t*);
int nvs_set_u32(nvs_handle_t,const char*,uint32_t); int nvs_erase_key(nvs_handle_t,const char*);
int nvs_commit(nvs_handle_t); void nvs_close(nvs_handle_t);
''',
        'esp_heap_caps.h': '''#pragma once
constexpr int MALLOC_CAP_INTERNAL=1, MALLOC_CAP_8BIT=2;
unsigned heap_caps_get_free_size(int); unsigned heap_caps_get_largest_free_block(int); unsigned heap_caps_get_minimum_free_size(int);
''',
        'esp_log.h': '#pragma once\n#define ESP_LOGE(...) ((void)0)\n#define ESP_LOGI(...) ((void)0)\n',
    }
    for name, text in headers.items():
        (work / name).write_text(text)
    (work / 'test.cpp').write_text(r'''
#include "input/ModeButton.h"
#include "transfer/Path.h"
#include "app/BootMode.h"
#include "esp_system.h"
#include "nvs.h"
#include <cassert>
#include <string>
#include <cstdio>
static bool exists=false, erasePending=false;
static uint32_t value=0;
static int commitError=0, initError=0, readError=0, resetReason=ESP_RST_SW;
static unsigned commits=0;
int nvs_flash_init(){return initError;}
int esp_reset_reason(){return resetReason;}
int nvs_open(const char*,int,nvs_handle_t* h){*h=1;return 0;}
int nvs_get_u32(nvs_handle_t,const char*,uint32_t* v){*v=value;return readError?readError:(exists?0:ESP_ERR_NVS_NOT_FOUND);}
int nvs_set_u32(nvs_handle_t,const char*,uint32_t v){value=v;exists=true;return 0;}
int nvs_erase_key(nvs_handle_t,const char*){erasePending=true;return 0;}
int nvs_commit(nvs_handle_t){++commits;if(commitError)return commitError;if(erasePending){exists=false;erasePending=false;}return 0;}
void nvs_close(nvs_handle_t){}
unsigned esp_get_minimum_free_heap_size(){return 0;}
unsigned esp_get_free_heap_size(){return 0;}
const char* esp_err_to_name(int){return "test error";}
unsigned heap_caps_get_free_size(int){return 0;}
unsigned heap_caps_get_largest_free_block(int){return 0;}
unsigned heap_caps_get_minimum_free_size(int){return 0;}
int main(int argc,char**argv){
 const std::string scenario=argc>1?argv[1]:"";
 if(scenario=="button-path"){
  ModeButton::Debouncer b;b.begin(false,0);
  assert(!b.update(false,30));assert(b.releasedStable(30));
  assert(!b.update(true,40));assert(!b.update(false,45));assert(!b.update(true,49));
  assert(!b.update(true,78));assert(!b.update(true,79));assert(!b.update(true,2000));
  assert(!b.update(false,2010));assert(!b.update(true,2015));assert(!b.update(false,2020));
  assert(!b.update(false,2049));assert(b.update(false,2050));assert(!b.update(false,2100));
  b.begin(true,0);assert(!b.update(true,100));assert(!b.update(false,120));assert(!b.update(false,150));
  assert(!b.update(true,170));assert(!b.update(true,200));assert(!b.update(false,220));assert(b.update(false,250));
  b.begin(false,UINT32_MAX-100);b.update(false,UINT32_MAX-70);
  b.update(true,UINT32_MAX-60);b.update(true,UINT32_MAX-30);b.update(false,UINT32_MAX-10);
  assert(!b.update(false,18));assert(b.update(false,19));
  char path[240];
  for(auto good:{"/","/assets/hero.bin","/hello+world.txt","/%C3%A4.bin","/a%2Fb"})assert(TransferPath::decode(good,path,sizeof(path)));
  for(auto bad:{"","a","//a","/a/","/../a","/a/%2e%2e/b","/%00a","/%2fetc","/a%5Cb","/a%3Ab","/a.","/a%20","/.transfer-ab.part","/.TRANSFER-ab.part","/%","/%1","/%GG","/%C0%AF","/%ED%A0%80","/%F4%90%80%80","/%80","/%C3","/a\x01"})assert(!TransferPath::decode(bad,path,sizeof(path)));
  assert(!TransferPath::decode(("/"+std::string(128,'a')).c_str(),path,sizeof(path)));
  assert(!TransferPath::decode(("/a/"+std::string(238,'b')).c_str(),path,sizeof(path)));
  assert(!TransferPath::decode("/abc",path,4));
 }else{
  if(scenario=="transfer" || scenario=="power" || scenario=="delete-error" || scenario=="invalid" || scenario=="wrong-type"){
   exists=true;value=scenario=="invalid"?0xffffffff:0x4d4f4401;
  }
  if(scenario=="power")resetReason=ESP_RST_POWERON;
  if(scenario=="wrong-type")readError=ESP_ERR_NVS_TYPE_MISMATCH;
  if(scenario=="delete-error")commitError=-1;
  if(scenario=="init-error")initError=-1;
  boot_mode_select();
  assert(boot_mode_get()==(scenario=="transfer"?BOOT_MODE_TRANSFER:BOOT_MODE_GAME));
  const unsigned count=commits;boot_mode_select();assert(commits==count);
  if(scenario=="transfer" || scenario=="power" || scenario=="invalid" || scenario=="wrong-type")assert(!exists && commits==1);
  if(scenario=="delete-error")assert(exists && boot_mode_error());
  if(scenario=="init-error")assert(!boot_mode_nvs_ready() && commits==0);
  if(scenario=="save") {assert(boot_mode_save(BOOT_MODE_TRANSFER));assert(exists&&value==0x4d4f4401&&commits==1);}
  if(scenario=="save-invalid"){assert(!boot_mode_save(static_cast<boot_mode_t>(2)));assert(commits==0);}
  if(scenario=="save-error"){commitError=-1;assert(!boot_mode_save(BOOT_MODE_TRANSFER));assert(boot_mode_error());}
 }
 printf("PASS %s\n",scenario.c_str());
}
''')
    executable = work / 'tests'
    subprocess.run(['g++', '-std=c++17', '-Wall', '-Wextra', '-Werror', '-Wno-unused-parameter',
                    '-DHOST_TEST', '-I'+str(work), '-I'+str(root/'main'),
                    str(work/'test.cpp'), str(root/'main/input/ModeButton.cpp'),
                    str(root/'main/transfer/Path.cpp'), str(root/'main/app/BootMode.cpp'),
                    '-o', str(executable)], check=True)
    for scenario in ['button-path', 'default', 'transfer', 'power', 'delete-error',
                     'invalid', 'wrong-type', 'init-error', 'save', 'save-error', 'save-invalid']:
        subprocess.run([str(executable), scenario], check=True)
