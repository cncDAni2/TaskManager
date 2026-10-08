# Ultra-könnyű Windows Feladatkezelő (TaskManager)

Az alkalmazás elsősorban a teendők gyors és egyszerű kezelésére szolgál, másodsorban pedig a munkaidőd nyomon követésére. Indítása a `TaskManager.exe` fájllal történik.

## Feladatok kezelése

- Ctrl+F1: Feladatkezelő főablak megnyitása
- Ctrl+F2: PIN mód bekapcsolása / kikapcsolása

A főablak megnyitásakor az új feladat felvételére szolgáló mező azonnal fókuszt kap. Nyomd meg a Ctrl+F1 billentyűkombinációt, írd be az új feladatot, majd nyomj Entert.  
Ha folyamatosan látni szeretnéd a feladataidat, a Ctrl+F2 billentyűkombinációval mini nézetet jeleníthetsz meg. Újabb Ctrl+F2-re a nézet eltűnik.  
**Helyi menü**: Az óra melletti ikonra jobb gombbal kattintva további funkciókat érhetsz el. Javasolt bekapcsolni az **Indítás a Windows-zal** lehetőséget.

### Pin nézet

A PIN nézet egy mini, kissé áttetsző lebegő ablak, amely mindig a képernyőn marad. A Ctrl+F2 billentyűkombinációval, a főablak PIN ikonjával vagy a helyi menüből kapcsolható be.  
**Jelölőnégyzetek:** Ebben a nézetben is késznek jelölheted a feladatokat. A kész feladatok még 10 másodpercig láthatók, így marad időd ellenőrizni vagy visszavonni a jelölést; ezután a PIN és a főablak aktív listájából is eltűnnek, és az „Elkészült” nézetben találhatók meg.
**Feladat címe:** Csak annyi szöveg látszik, amennyi kifér. Kattints a címre a főablak megnyitásához és a feladat szerkesztéséhez; a teljes szöveg ki lesz jelölve.
**Jelölések:** Látható a feladat jelölőikonja, globálisan szinkronizált feladatnál pedig a felhőikon és az „ÉN” gomb.  
**Felső fülek:** Itt látható a visszaszámlálás (piros háttér = munka, kék = pihenés, szem ikon = szem pihentetése), a Fókusz mód kapcsolója és a lebegő ablak mozgatófogantyúja.

### Főablak nézet - alapértelmezett nézet

A főablak megnyitásakor az új feladat hozzáadására szolgáló mező azonnal aktív. Mellette felhőikon található. A globális szinkronizáláshoz az új feladat létrehozása előtt kapcsold be a felhőikont.  
A feladatokat szerkesztheted, törölheted, készre jelölheted, jelölőikonnal láthatod el (az ikonra kattintva válthatsz a jelölések között), és húzással rendezheted sorba.  
Főablakban készre jelölve a feladat az ablak bezárásáig látható marad az aktív listában; újranyitáskor már csak az „Elkészült” nézetben szerepel.
A kész feladatokat napi bontásban, a fenti „Elkészült” gombra kattintva láthatod.

**Gyorsbillentyűk** a főablak nézetben:

- Enter: új feladat hozzáadása, ha a beviteli mező aktív.  
- Tab / Shift+Tab: Váltás a feladatok között
- Space: A kijelölt feladat készre jelölése vagy a jelölés visszavonása
- Enter: A kijelölt feladat szerkesztése, illetve a szerkesztés befejezése és mentése
- ESC: Szerkesztés közben visszavonja a módosításokat; egyébként befejezi és menti a szerkesztést
- Delete: Kijelölt feladat törlése (megerősítést kér)
- Dupla kattintás egy feladatra: az adott feladat szerkesztése.

### Szinkronizált feladatok

A beállításokban megadható a szinkronizálási fájl helye. A program nem tölti fel a fájlt a hálózatra: annak megosztott helyen kell lennie (például OneDrive- vagy Google Drive-mappában), a szinkronizálást pedig a tárhelyszolgáltató alkalmazása végzi.  

A szinkronizált fájl speciális:

- Felhőikont kap.
- Csak a megosztott helyen található fájlt használja.
- Minden változtatás azonnal mentésre kerül a fájlba. Ha többen egyszerre módosítják, egy későbbi szinkronizálás felülírhatja a változtatásokat.
- Nem jelölhető meg speciális ikonnal
- Megjelenik, hogy ki vette fel a feladatot.
- Az "ÉN" gombbal magadra veheted a feladatot. A többiek látják, ki vette fel, és a feladatot piros szín jelzi. Egy feladatot csak egy ember vehet fel; a többiek számára zárolva lesz.
- A feladatok sorrendje módosítható, de csak nálad érvényes; mások nem látják.

## Időmérés

Nem ez a program fő funkciója, de hasznos a munkaidő nyomon követésére is. A képernyő előtt töltött aktív időt méri. A mérés szünetel, ha:

- Két perce nem volt billentyű- vagy egéraktivitás.
- A rendszer zárolva van.
- Manuálisan szüneteltetted az időmérést
- Az előtérben lévő ablak címe tartalmazza a Discord, YouTube vagy Facebook nevek egyikét.
- Bezártad a programot.
- Az időzítő "STRICT MODE"-ban van, és éppen pihenőidőt mér.

Az időmérés "munkanapot" mér. A munkaidő kezdete alapértelmezés szerint 9:15.  
Az eddigi méréseket napi és heti bontásban is megtekintheted az "Idők" oldalon.  
**Manuális** munkaidőmegadás: Az Idők nézetben kézzel is megadhatsz munkaidőt. Kiválaszthatsz egy napot, ahol módosíthatod a korábbi kézi bejegyzést. Alapból minden naphoz ez 0 óra. A kézi megadást az áttekintő nézetek más színnel jelzik.  
A százalékos kijelző a 8 órás munkaidőhöz viszonyítva mutatja az eltelt aktív idő arányát. Ez törvényileg szabályozott módon 6 óra 50 perc-nél jelez 100%-ot, ugyanis a törvény egy 20 perces szünetet ír elő, valamint hogy a képernyő előtti munkavégzést óránként 10 percre szüneteltetni kell. (Fontos megjegyezni: Ez NEM munkaszünet, de a program a képernyőidőt méri, azt pedig szüneteltetni kell - így a program számára ez nem számolható mint munkaidő.)

### Időzítő: Munka/pihenés

Az időzítő funkció segít az előző részben leírt munka- és pihenőciklusok betartásában. Kattints a főablakban a stopperóra ikonra. Itt beállíthatod a munka- és pihenőidő hosszát. A Start és a Pause gombbal elindíthatod, illetve szüneteltetheted az időzítőt; a Következő gombbal pedig azonnal továbbléphetsz a következő szakaszra. A program hangjelzéssel és a PIN ablak rövid villogtatásával jelzi a váltást.

STRICT MODE: Ha ezt is bepipálod, akkor az időzítő kihatással lesz a munkaidő mérésre, és automatikusan szünetelteti a munkaidő mérést a pihenőidő alatt.

20-20-20 szabály: A szem egészségének megőrzése érdekében minden 20 perc képernyőnézés után nézz 20 másodpercig 20 láb (kb. 6 méter) távolságra. A szabály a munkaidő-méréstől és az intervallum-időzítőtől függetlenül működik, YouTube, Discord és Facebook használata közben is. Két perc billentyű- vagy egérinaktivitás után szünetel, kivéve fókusz módban; a gép zárolásakor mindig szünetel. Rövid hangjelzéssel emlékeztet a program, amikor ideje szünetet tartani (2 csippanás), és mikor vissza kell térni(1 csippanás). A PIN ablakban a számláló szem ikonra vált a szem-pihenő alatt.

### Fókusz mód

Fókusz mód a főablakban és a PIN ablakban is elérhető (célkereszt ikon). Bekapcsolásakor a program az inaktivitás és a kizárt alkalmazások ellenére is méri az időt, amíg a gépet le nem zárolod, vagy ki nem kapcsolod a fókusz módot. A bekapcsolt mód ikonja kék.

## Erőforrásigény és fordítás

Közel 0 erőforrásigény:

- Elsődleges cél a minimális erőforrásigény biztosítása.
- Tiszta Win32 C++ (nincs keretrendszer, nincs háttérben pörgő felesleges szál vagy timer).
- Eseményvezérelt `GetMessage` üzenetciklus (0% CPU tétlen állapotban).
- Minimális memóriahasználat: 800KB - 2MB.
- Elrejtéskor a rendszer automatikusan kiüríti a memóriát (`SetProcessWorkingSetSize`), így az alkalmazás minimális memóriát foglal.
- Nincsenek külső hangfájlok: az alkalmazás memóriában generálja a hangokat.

A mellékelt `build.bat` parancsfájl azonnal lefordítja az alkalmazást:

```cmd
build.bat
```

A generált futtatható fájl: `TaskManager.exe` (~550 KB méretű önálló natív exe). A fordítás köztes fájljai a `dist/` mappába kerülnek.
