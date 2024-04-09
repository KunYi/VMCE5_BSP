!if 0
Copyright (c) Microsoft Corporation.  All rights reserved.
!endif
!if 0
Use of this source code is subject to the terms of the Microsoft end-user
license agreement (EULA) under which you licensed this SOFTWARE PRODUCT.
If you did not accept the terms of the EULA, you are not authorized to use
this source code. For a copy of the EULA, please see the LICENSE.RTF on your
install media.
!endif
!IF 0


Module Name:

    makefile.inc

!ENDIF

# Take the sboot.exe that resulted from the standard build and generate
# sboot.bin from it.

RomImage:
!IF "$(NOLINK)" == ""    
	romimage boot.bib
!ENDIF        
!IF "$(WINCEREL)"=="1"
	copy $(_TARGETPLATROOT)\target\$(_TGTCPU)\$(WINCEDEBUG)\sboot.* $(_FLATRELEASEDIR)
!ENDIF        