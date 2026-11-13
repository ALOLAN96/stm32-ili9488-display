"""Build real submodule LVGL plus integration, then execute tests in fresh processes.
Windows Clang: --headers provides C library headers; no simulator or fake LVGL API.
"""
import argparse,concurrent.futures,ctypes,os,re,subprocess,sys
from pathlib import Path
R=Path(__file__).resolve().parents[1]
def run(cmd):
 p=subprocess.run([str(x) for x in cmd],capture_output=True,text=True,timeout=120)
 if p.returncode: raise RuntimeError(p.stdout+p.stderr)
 return p.stdout

def main():
 a=argparse.ArgumentParser();a.add_argument('--cc',required=True);a.add_argument('--headers',required=True);a.add_argument('--extra-headers',required=True);a.add_argument('--arm',action='store_true');a.add_argument('--worker',nargs=2);args=a.parse_args()
 if args.worker:
  lib=ctypes.CDLL(args.worker[0]);f=getattr(lib,args.worker[1]);f.restype=ctypes.c_int;r=f();print(args.worker[1], 'PASS' if r==0 else 'FAIL line '+str(r));return int(r!=0)
 dependency = R / 'User/ThirdParty/lvgl'
 if not (dependency / 'lvgl.h').is_file() or not (dependency / 'src/lv_init.c').is_file():
  raise RuntimeError('LVGL submodule is missing. From the parent repository run: git submodule update --init -- User/ThirdParty/lvgl')
 out=R/'build'/('lvgl-arm' if args.arm else 'lvgl-host');out.mkdir(parents=True,exist_ok=True); (out/'importgen').mkdir(exist_ok=True)
 sources=sorted((R/'User/ThirdParty/lvgl/src').rglob('*.c'))
 sources += [R/p for p in ['User/Services/Display/svc_lvgl_display.c','User/App/LvglDemo/app_lvgl_demo.c','User/Device/ILI9488/drv_ili9488.c','User/Device/ILI9488/drv_ili9488_panel.c']]
 if not args.arm: sources += [R/p for p in ['tests/test_lvgl.c','tests/mock_port.c','tests/fixtures/panel_test.c','tests/runtime.c']]
 flags=['-std=c99','-ffreestanding','-fno-builtin','-fno-stack-protector','-O1','-DLV_CONF_INCLUDE_SIMPLE']
 flags+=['-I'+str(R/p) for p in ['User/Config','User/ThirdParty/lvgl','User/Services/Display','User/App/LvglDemo','User/Device/ILI9488','tests']]
 flags+=['-I'+args.headers,'-idirafter',args.extra_headers]
 if args.arm:flags+=['-target','arm-none-eabi','-mcpu=cortex-m4','-mthumb']
 else:flags+=['-mno-stack-arg-probe']
 def compile_unit(item):
  i,s=item;o=out/(str(i)+'.o');run([args.cc,*flags,'-c',s,'-o',o]);return o
 with concurrent.futures.ThreadPoolExecutor(max_workers=8) as pool: objects=list(pool.map(compile_unit,enumerate(sources)))
 print('PASS compiled',len(objects),'actual LVGL/integration units',flush=True)
 if args.arm:return 0
 linker=Path(args.cc).with_name('lld.exe')
 # Windows CRT import library, host only. Vendor and product sources remain untouched.
 exports=['abort','exit','memcpy','memset','memmove','memcmp','strlen','strcmp','strcpy','strchr','strstr','strncmp','strncpy','strcat','strtol','strtoul','atoi','abs','rand','srand','malloc','free','realloc','calloc','floor','floorf','ceil','ceilf','sqrt','sqrtf','pow','powf','round','roundf','fmod','fmodf','sin','sinf','cos','cosf','log','logf','exp','expf','atan2','atan2f']
 definition=out/'crt.def';definition.write_text('LIBRARY msvcrt.dll\nEXPORTS\n'+'\n'.join(exports)+'\n')
 run([linker,'-flavor','link','/dll','/noentry','/force:unresolved','/def:'+str(definition),'/machine:x64','/out:'+str(out/'importgen/msvcrt.dll'),'/implib:'+str(out/'crt.lib')])
 extra=out/'float.c';extra.write_text('int _fltused=0;\n');extra_obj=out/'float.o';run([args.cc,*flags,'-c',extra,'-o',extra_obj])
 dll=out/'tests.dll';run([linker,'-flavor','link','/dll','/noentry','/nodefaultlib','/opt:ref','/out:'+str(dll),*objects,extra_obj,out/'crt.lib'])
 cases=re.findall(r'^TEST\((\w+)\)',(R/'tests/test_lvgl.c').read_text(encoding='utf-8'),re.M)
 for case in cases:
  subprocess.run([sys.executable,__file__,'--cc',args.cc,'--headers',args.headers,'--extra-headers',args.extra_headers,'--worker',str(dll),case],check=True,timeout=60)
 print('PASS',len(cases),'real LVGL runtime cases');return 0
if __name__=='__main__':sys.exit(main())
