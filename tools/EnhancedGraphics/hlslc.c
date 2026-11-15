/* hlslc: compile HLSL to D3D9 bytecode via d3dcompiler_47 (Wine/vkd3d or Windows). */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
typedef struct ID3DBlobVtbl { HRESULT (WINAPI*QI)(void*,REFIID,void**); ULONG (WINAPI*AddRef)(void*); ULONG (WINAPI*Release)(void*); void* (WINAPI*GetBufferPointer)(void*); SIZE_T (WINAPI*GetBufferSize)(void*); } ID3DBlobVtbl;
typedef struct ID3DBlob { ID3DBlobVtbl* v; } ID3DBlob;
typedef HRESULT (WINAPI *PFN)(const void*,SIZE_T,const char*,const void*,void*,const char*,const char*,UINT,UINT,ID3DBlob**,ID3DBlob**);
typedef HRESULT (WINAPI *PFND)(const void*,SIZE_T,UINT,const char*,ID3DBlob**);
int main(int argc,char**argv){
  if(argc<5){fprintf(stderr,"usage: hlslc in.hlsl entry profile out.bin [out.asm]\n");return 2;}
  HMODULE m=LoadLibraryA("d3dcompiler_47.dll"); if(!m){fprintf(stderr,"no d3dcompiler\n");return 3;}
  PFN c=(PFN)GetProcAddress(m,"D3DCompile"); PFND d=(PFND)GetProcAddress(m,"D3DDisassemble");
  FILE*f=fopen(argv[1],"rb"); fseek(f,0,2); long n=ftell(f); fseek(f,0,0); char*s=malloc(n+1); fread(s,1,n,f); s[n]=0; fclose(f);
  ID3DBlob*code=0,*err=0;
  HRESULT hr=c(s,n,argv[1],0,0,argv[2],argv[3],(1<<15)/*OPTIMIZATION_LEVEL3*/,0,&code,&err);
  if(err){fprintf(stderr,"%.*s\n",(int)err->v->GetBufferSize(err),(char*)err->v->GetBufferPointer(err));}
  if(FAILED(hr)||!code){fprintf(stderr,"compile failed hr=%08lx\n",hr);return 1;}
  f=fopen(argv[4],"wb"); fwrite(code->v->GetBufferPointer(code),1,code->v->GetBufferSize(code),f); fclose(f);
  if(argc>5&&d){ID3DBlob*a=0; if(SUCCEEDED(d(code->v->GetBufferPointer(code),code->v->GetBufferSize(code),0,0,&a))&&a){f=fopen(argv[5],"wb");fwrite(a->v->GetBufferPointer(a),1,a->v->GetBufferSize(a)-1,f);fclose(f);}}
  printf("ok %lu bytes\n",(unsigned long)code->v->GetBufferSize(code)); return 0;}
