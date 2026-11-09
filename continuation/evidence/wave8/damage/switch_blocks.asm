
# Static raw-image range 0x413708da..0x41370981
413708da: ff5030                           call DWORD PTR [eax+0x30]
413708dd: d95dd0                           fstp DWORD PTR [ebp-0x30]
413708e0: 4b                               dec ebx
413708e1: 83fb07                           cmp ebx,0x7
413708e4: 0f87de000000                     ja 0x413709c8
413708ea: f30f104dd0                       movss xmm1,DWORD PTR [ebp-0x30]
413708ef: ff249d2cdc0642                   jmp DWORD PTR [ebx*4+0x4206dc2c]
413708f6: f30f590d58db0642                 mulss xmm1,DWORD PTR ds:0x4206db58
413708fe: e995010000                       jmp 0x41370a98
41370903: 64a130000000                     mov eax,fs:0x30
41370909: 8b88a8000000                     mov ecx,DWORD PTR [eax+0xa8]
4137090f: 0388a4000000                     add ecx,DWORD PTR [eax+0xa4]
41370915: 0388b8000000                     add ecx,DWORD PTR [eax+0xb8]
4137091b: 0388bc000000                     add ecx,DWORD PTR [eax+0xbc]
41370921: 034864                           add ecx,DWORD PTR [eax+0x64]
41370924: 83fe03                           cmp esi,0x3
41370927: 0f85a8000000                     jne 0x413709d5
4137092d: b83fc798f2                       mov eax,0xf298c73f
41370932: c1c920                           ror ecx,0x20
41370935: c1ce20                           ror esi,0x20
41370938: 051c4a5f00                       add eax,0x5f4a1c
4137093d: 0598bcaa20                       add eax,0x20aabc98
41370942: 3522b02f05                       xor eax,0x52fb022
41370947: 2db17d8d16                       sub eax,0x168d7db1
4137094c: 90                               nop
4137094d: 90                               nop
4137094e: 90                               nop
4137094f: 90                               nop
41370950: 8945e0                           mov DWORD PTR [ebp-0x20],eax
41370953: 394de0                           cmp DWORD PTR [ebp-0x20],ecx
41370956: 7438                             je 0x41370990
41370958: 88f6                             mov dh,dh
4137095a: 0fbec9                           movsx ecx,cl
4137095d: 6601c1                           add cx,ax
41370960: 6681ed5c31                       sub bp,0x315c
41370965: 66d3c5                           rol bp,cl
41370968: c1cb14                           ror ebx,0x14
4137096b: 66c1e909                         shr cx,0x9
4137096f: 66c1e718                         shl di,0x18
41370973: 0fca                             bswap edx
41370975: 31c2                             xor edx,eax
41370977: 660d1205                         or ax,0x512
4137097b: 6621ef                           and di,bp
4137097e: f9                               stc
4137097f: ffe2                             jmp edx

# Static raw-image range 0x41370990..0x413709d5
41370990: b81190ed8a                       mov eax,0x8aed9011
41370995: 0580108c3e                       add eax,0x3e8c1080
4137099a: c1c807                           ror eax,0x7
4137099d: 350c461e78                       xor eax,0x781e460c
413709a2: c1c80f                           ror eax,0xf
413709a5: c1cf20                           ror edi,0x20
413709a8: c1c008                           rol eax,0x8
413709ab: 2d2a989d6c                       sub eax,0x6c9d982a
413709b0: 90                               nop
413709b1: 90                               nop
413709b2: 90                               nop
413709b3: 8945e0                           mov DWORD PTR [ebp-0x20],eax
413709b6: e990000000                       jmp 0x41370a4b
413709bb: f30f590d5cdb0642                 mulss xmm1,DWORD PTR ds:0x4206db5c
413709c3: e9d0000000                       jmp 0x41370a98
413709c8: f30f100d54a50542                 movss xmm1,DWORD PTR ds:0x4205a554
413709d0: e9c3000000                       jmp 0x41370a98

# Static raw-image range 0x413709d5..0x41370a25
413709d5: b82d8befd7                       mov eax,0xd7ef8b2d
413709da: c1ca20                           ror edx,0x20
413709dd: c1c80f                           ror eax,0xf
413709e0: c1c017                           rol eax,0x17
413709e3: c1c810                           ror eax,0x10
413709e6: c1c01c                           rol eax,0x1c
413709e9: c1c007                           rol eax,0x7
413709ec: 2d50701969                       sub eax,0x69197050
413709f1: 35290ca605                       xor eax,0x5a60c29
413709f6: eb00                             jmp 0x413709f8
413709f8: 8945e0                           mov DWORD PTR [ebp-0x20],eax
413709fb: 394de0                           cmp DWORD PTR [ebp-0x20],ecx
413709fe: 7425                             je 0x41370a25
41370a00: 6689db                           mov bx,bx
41370a03: 0fb6c3                           movzx eax,bl
41370a06: 6681c76655                       add di,0x5566
41370a0b: 28de                             sub dh,bl
41370a0d: 66c1c21f                         rol dx,0x1f
41370a11: d3ce                             ror esi,cl
41370a13: 66d3ed                           shr bp,cl
41370a16: 66d3e5                           shl bp,cl
41370a19: 0fcd                             bswap ebp
41370a1b: 30e9                             xor cl,ch
41370a1d: 09f0                             or eax,esi
41370a1f: 6621eb                           and bx,bp
41370a22: fa                               cli
41370a23: ffe3                             jmp ebx

# Static raw-image range 0x41370a25..0x41370ab1
41370a25: b8efadd2df                       mov eax,0xdfd2adef
41370a2a: 35ed4df370                       xor eax,0x70f34ded
41370a2f: c1cf20                           ror edi,0x20
41370a32: c1c00b                           rol eax,0xb
41370a35: 351fad4007                       xor eax,0x740ad1f
41370a3a: c1c81e                           ror eax,0x1e
41370a3d: c1c008                           rol eax,0x8
41370a40: c1c004                           rol eax,0x4
41370a43: c1cb20                           ror ebx,0x20
41370a46: eb00                             jmp 0x41370a48
41370a48: 8945e0                           mov DWORD PTR [ebp-0x20],eax
41370a4b: 8b4de0                           mov ecx,DWORD PTR [ebp-0x20]
41370a4e: 8b01                             mov eax,DWORD PTR [ecx]
41370a50: ff5030                           call DWORD PTR [eax+0x30]
41370a53: d95dd4                           fstp DWORD PTR [ebp-0x2c]
41370a56: f30f104dd4                       movss xmm1,DWORD PTR [ebp-0x2c]
41370a5b: a184085042                       mov eax,ds:0x42500884
41370a60: 8b0d40855142                     mov ecx,DWORD PTR ds:0x42518540
41370a66: 648b152c000000                   mov edx,DWORD PTR fs:0x2c
41370a6d: 8b0c8a                           mov ecx,DWORD PTR [edx+ecx*4]
41370a70: 3b8108000000                     cmp eax,DWORD PTR [ecx+0x8]
41370a76: 0f8fcf000000                     jg 0x41370b4b
41370a7c: f30f104510                       movss xmm0,DWORD PTR [ebp+0x10]
41370a81: a150085042                       mov eax,ds:0x42500850
41370a86: 803c3800                         cmp BYTE PTR [eax+edi*1],0x0
41370a8a: 7408                             je 0x41370a94
41370a8c: f30f590d18a50542                 mulss xmm1,DWORD PTR ds:0x4205a518
41370a94: f30f59c8                         mulss xmm1,xmm0
41370a98: 8b45e8                           mov eax,DWORD PTR [ebp-0x18]
41370a9b: 64a300000000                     mov fs:0x0,eax
41370aa1: f30f114dd8                       movss DWORD PTR [ebp-0x28],xmm1
41370aa6: d945d8                           fld DWORD PTR [ebp-0x28]
41370aa9: 83c424                           add esp,0x24
41370aac: 5e                               pop esi
41370aad: 5f                               pop edi
41370aae: 5b                               pop ebx
41370aaf: 5d                               pop ebp
41370ab0: c3                               ret

# Static raw-image range 0x41370b4b..0x41370b6f
41370b4b: 6884085042                       push 0x42500884
41370b50: f30f114ddc                       movss DWORD PTR [ebp-0x24],xmm1
41370b55: e81071a300                       call 0x41da7c6a
41370b5a: f30f104ddc                       movss xmm1,DWORD PTR [ebp-0x24]
41370b5f: 83c404                           add esp,0x4
41370b62: 833d84085042ff                   cmp DWORD PTR ds:0x42500884,0xffffffff
41370b69: 0f850dffffff                     jne 0x41370a7c
