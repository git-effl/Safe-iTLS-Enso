# Safe iTLS-Enso 

A Plugin that intercepts TLS calls from vs0: to ux0: or uma0:,a safer option to the original iTLS-Enso.

## How to build

Requirements:

- WSL (you can even use MSys2)
- VitaSDK
- Git

1. Run ```git clone https://github.com/git-effl/Safe-iTLS-Enso.git```
2. Run ```make```
3. You're done

## Setup:

Requirements:

- A Modded Vita on 3.60-3.65 with Enso
- Vitashell/MolecularShell/EnsoShell
- An official update file
- The Plugin

1. Get your [official update file](http://deu01.psv.update.playstation.net/update/psv/image/2022_0209/rel_f2c7b12fe85496ec88a0391b514d6e3b/PSVUPDAT.PUP) and the plugin from the releases,if you use a SD2Vita,use 1.00 uma0 version,if not use normal 1.00.
2. Put it somewhere on your SD/Memory Card.
3. Vitashell/MolecularShell/EnsoShell.
4. Start FTP.
5. Put the plugin file on the tai folder on your SD/Memory Card
6. Write this snippet on your taiHEN configuration file: 
```*KERNEL
ur0:tai/safe-itls-enso.skprx```
7. Close FTP.
8. Extract the PUP file 
9. Get files libhttp.suprx,libhttp.suprx,np_commerce2.suprx,np_commerce2.suprx from your card directory/sys/external
10. Get File ScePsp2Compat.suprx from your card directory/data/external/webcore
11. Get the file CA_LIST.cer from your card directory/data/external/cert/
12. Put all the Files you got from your firmware file to your card directory/itls
13. Reboot your console and you're done!

## License
Licensed under GPL3,see LICENSE for more details.
