           ☠️
           ██╗██████╗ ██╗     ███████╗███████╗███████╗██╗███╗   ██╗ ██████╗
           ██║██╔══██╗██║     ██╔════╝██╔════╝██╔════╝██║████╗  ██║██╔════╝
           ██║██████╔╝██║     █████╗  ███████╗███████╗██║██╔██╗ ██║██║  ███╗
           ██║██╔══██╗██║     ██╔══╝  ╚════██║╚════██║██║██║╚██╗██║██║   ██║
           ██║██████╔╝███████╗███████╗███████║███████║██║██║ ╚████║╚██████╔╝
           ╚═╝╚═════╝ ╚══════╝╚══════╝╚══════╝╚══════╝╚═╝╚═╝  ╚═══╝ ╚═════╝

# iblessing
- `iblessing` is an iOS security exploiting toolkit, it mainly includes **application information collection**, **static analysis** and **dynamic analysis**.
- `iblessing` is based on [unicorn engine](https://github.com/unicorn-engine/unicorn) and [capstone engine](https://github.com/aquynh/capstone).

# How to Compile
To get started compiling iblessing, please follow the steps below:
```
git clone https://github.com/Soulghost/iblessing
cd iblessing
sh compile.sh
```

If all of this run successfully, you can find the binary in build directory:
```
> ls ./build
iblessing

> file ./build/iblessing
./build/iblessing: Mach-O 64-bit executable x86_64
```

# Documentation & Help
## Preview
```
$ iblessing -h

           ☠️
           ██╗██████╗ ██╗     ███████╗███████╗███████╗██╗███╗   ██╗ ██████╗
           ██║██╔══██╗██║     ██╔════╝██╔════╝██╔════╝██║████╗  ██║██╔════╝
           ██║██████╔╝██║     █████╗  ███████╗███████╗██║██╔██╗ ██║██║  ███╗
           ██║██╔══██╗██║     ██╔══╝  ╚════██║╚════██║██║██║╚██╗██║██║   ██║
           ██║██████╔╝███████╗███████╗███████║███████║██║██║ ╚████║╚██████╔╝
           ╚═╝╚═════╝ ╚══════╝╚══════╝╚══════╝╚══════╝╚═╝╚═╝  ╚═══╝ ╚═════╝

[***] iblessing iOS Security Exploiting Toolkit Beta 0.1.1 (http://blog.asm.im)
[***] Author: Soulghost (高级页面仔) @ (https://github.com/Soulghost)

Usage: iblessing [options...]
Options:
    -m, --mode             mode selection:
                                * scan: use scanner
                                * generator: use generator
    -i, --identifier       choose module by identifier:
                                * <scanner-id>: use specific scanner
                                * <generator-id>: use specific generator
    -f, --file             input file path
    -o, --output           output file path
    -l, --list             list available scanners
    -d, --data             extra data
    -h, --help             Shows this page
```

## Basic Concepts
### Scanner
A scanner is a component used to output analysis report through static and dynamic analysis of binary files, for example, the objc-msg-xref scanner can dynamiclly analyze most objc_msgSend cross references.

```
[*] Scanner List:
    - app-info: extract app infos
    - objc-class-xref: scan for class xrefs
    - objc-msg-xref: generate objc_msgSend xrefs record
    - predicate: scan for NSPredicate xrefs and sql injection surfaces
    - symbol-wrapper: detect symbol wrappers
```

### Generator
A generator is a component that performs secondary processing on the report generated by the scanner, for example, it can generate IDA scripts based on the the objc-msg-xref scanner's cross references report.

```
[*] Generator List:
    - ida-objc-msg-xref: generator ida scripts to add objc_msgSend xrefs from objc-msg-xref scanner's report
    - objc-msg-xref-server: server to query objc-msg xrefs
```

## Basic Usage
### Scan for AppInfos
```
> iblessing -m scan -i app-info -f <path-to-app-bundle>
```

Let's take WeChat as an example:
```
> iblessing -m scan -i app-info -f WeChat.app
[*] set output path to /opt/one-btn/tmp/apps/WeChat/Payload
[*] input file is WeChat.app
[*] start App Info Scanner
[+] find default plist file Info.plist!
[*] find version info: Name: 微信(WeChat)
Version: 7.0.14(18E226)
ExecutableName: WeChat
[*] Bundle Identifier: com.tencent.xin
[*] the app allows HTTP requests **without** exception domains!
[+] find app deeplinks
 |-- wechat://
 |-- weixin://
 |-- fb290293790992170://
 |-- weixinapp://
 |-- prefs://
 |-- wexinVideoAPI://
 |-- QQ41C152CF://
 |-- wx703://
 |-- weixinULAPI://
[*] find app callout whitelist
 |-- qqnews://
 |-- weixinbeta://
 |-- qqnewshd://
 |-- qqmail://
 |-- whatsapp://
 |-- wxwork://
 |-- wxworklocal://
 |-- wxcphonebook://
 |-- mttbrowser://
 |-- mqqapi://
 |-- mqzonev2://
 |-- qqmusic://
 |-- tenvideo2://
 ...
[+] find 507403 string literals in binary
[*] process with string literals, this maybe take some time
[+] find self deeplinks URLs:
 |-- weixin://opennativeurl/devicerankview
 |-- weixin://dl/offlinepay/?appid=%@
 |-- weixin://opennativeurl/rankmyhomepage
 ...
 [+] find other deeplinks URLs:
 |-- wxpay://f2f/f2fdetail
 |-- file://%@?lang=%@&fontRatio=%.2f&scene=%u&version=%u&type=%llu&%@=%d&qqFaceFolderPath=%@&platform=iOS&netType=%@&query=%@&searchId=%@&isHomePage=%d&isWeAppMore=%d&subType=%u&extParams=%@&%@=%@&%@=%@
 ...
 [*] write report to path /opt/one-btn/tmp/apps/WeChat/Payload/WeChat.app_info.iblessing.txt
 
> ls -alh 
-rw-r--r--@ 1 soulghost  wheel    29K Jul 23 14:01 WeChat.app_info.iblessing.txt
```

### Scan for Class XREFs
***Notice: ARM64 Binaries Only***
```
iblessing -m scan -i objc-class-xref -f <path-to-binary> -d 'classes=<classname_to_scan>,<classname_to_scan>,...'
```

```
> restore-symbol WeChat -o WeChat.restored
> iblessing -m scan -i objc-class-xref -f WeChat.restored -d 'classes=NSPredicate'
[*] set output path to /opt/one-btn/tmp/apps/WeChat/Payload
[*] input file is WeChat
[+] detect mach-o header 64
[+] detect litten-endian
[*] start Objc Class Xref Scanner
  [*] try to find _OBJC_CLASS_$_NSPredicate
  [*] Step 1. locate class refs
	[+] find _OBJC_CLASS_$_NSPredicate at 0x108eb81d8
  [*] Step 2. find __TEXT,__text
	[+] find __TEXT,__text at 0x4000
  [*] Step 3. scan in __text
	[*] start disassembler at 0x100004000
	[*] \ 0x1002e1a50/0x1069d9874 (2.71%)	[+] find _OBJC_CLASS_$_NSPredicate ref at 0x1002e1a54
           ...
  [*] Step 4. symbolicate ref addresses
           [+] _OBJC_CLASS_$_NSPredicate -|
           [+] find _OBJC_CLASS_$_NSPredicate ref -[WCWatchNotificationMgr addYoCount:contact:type:] at 0x1002e1a54
           [+] find _OBJC_CLASS_$_NSPredicate ref -[NotificationActionsMgr handleSendMsgResp:] at 0x1003e0e28
           [+] find _OBJC_CLASS_$_NSPredicate ref -[FLEXClassesTableViewController searchBar:textDidChange:] at 0x1004a090c
           [+] find _OBJC_CLASS_$_NSPredicate ref +[GameCenterUtil parameterValueForKey:fromQueryItems:] at 0x1005a823c
           [+] find _OBJC_CLASS_$_NSPredicate ref +[GameCenterUtil getNavigationBarColorForUrl:defaultColor:] at 0x1005a8cd8
           ...
```

### Scan for All objc_msgSend XREFs
***Notice: ARM64 Binaries Only***

#### Simple Mode
```
iblessing -m scan -i objc-msg-xref -f <path-to-binary>
```

#### Anti-Wrapper Mode
```
iblessing -m scan -i objc-msg-xref -f WeChat -d 'antiWrapper=1'
```
The anti-wrapper mode will detect objc_msgSend wrappers and make transforms, such as:
```arm
; __int64 __usercall objc_msgSend_X0_X22_X20@<X0>(void *obj@<X0>, const char *sel@<X22>, id anyObj@<X20>, ...)
objc_msgSend_X0_X22_X20:
MOV             X1, X22
MOV             X2, X20
B               objc_msgSend
```

Usage Example:
```
> iblessing -m scan -i objc-msg-xref -f WeChat -d 'antiWrapper=1'
[*] set output path to /opt/one-btn/tmp/apps/WeChat/Payload
[*] input file is WeChat
[+] detect mach-o header 64
[+] detect litten-endian

[*] !!! Notice: enter anti-wrapper mode, start anti-wrapper scanner
[*] start Symbol Wrapper Scanner
  [*] try to find wrappers for_objc_msgSend
  [*] Step1. find __TEXT,__text
	[+] find __TEXT,__text at 0x100004000
	[+] mapping text segment 0x100000000 ~ 0x107cb0000 to unicorn engine
  [*] Step 2. scan in __text
	[*] start disassembler at 0x100004000
	[*] / 0x1069d986c/0x1069d9874 (100.00%)
	[*] reach to end of __text, stop
  [+] anti-wrapper finished
  
[*] start ObjcMethodXrefScanner Exploit Scanner
  [*] Step 1. realize all app classes
	[*] realize classes 14631/14631 (100.00%)
	[+] get 667318 methods to analyze
  [*] Step 2. dyld load non-lazy symbols
  [*] Step 3. track all calls
	[*] progress: 667318 / 667318 (100.00%)
  [*] Step 4. serialize call chains to file
  [*] saved to /opt/one-btn/tmp/apps/WeChat/Payload/WeChat_method-xrefs.iblessing.txt
  
> ls -alh WeChat_method-xrefs.iblessing.txt
-rw-r--r--  1 soulghost  wheel    63M Jul 23 14:46 WeChat_method-xrefs.iblessing.txt 

> head WeChat_method-xrefs.iblessing.txt
iblessing methodchains,ver:0.2;
chainId,sel,prefix,className,methodName,prevMethods,nextMethods
182360,0x1008a0ab8,+[A8KeyControl initialize],+,A8KeyControl,initialize,[],[4429#0x1008a1064@4376#0x1008a1050@13769#0x1008a10d0]
182343,0x1008a0ad0,+[A8KeyControl_QueryStringTransferCookie initialize],+,A8KeyControl_QueryStringTransferCookie,initialize,[],[4429#0x1008a1064@4376#0x1008a1050@13769#0x1008a10d0]
145393,0x1008c2220,+[A8KeyResultCookieWriter initWithDomain:weakWebView:andCompleteBlock:],+,A8KeyResultCookieWriter,initWithDomain:weakWebView:andCompleteBlock:,[145386#0x10036367c],[]
145396,0x1008c3df8,+[A8KeyResultCookieWriter setA8KeyCookieExpireTime:],+,A8KeyResultCookieWriter,setA8KeyCookieExpireTime:,[145386#0x1003636e8],[]
145397,0x1008c27e8,+[A8KeyResultCookieWriter writeCompleteMarkerCookieValue:forKey:],+,A8KeyResultCookieWriter,writeCompleteMarkerCookieValue:forKey:,[145386#0x10036380c],[]
253456,0x0,+[AAOperationReq init],+,AAOperationReq,init,[253455#0x1039a9d30],[]
253457,0x0,+[AAOperationReq setBaseRequest:],+,AAOperationReq,setBaseRequest:,[253455#0x1039a9d8c],[]
186847,0x0,+[AAOperationRes length],+,AAOperationRes,length,[186845#0x10342aa54],[]
```

