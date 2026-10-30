# Ultra-könnyű Windows Feladatkezelő (TaskManager)

Az alkalmazás elsősorban a teendők gyors és egyszerű kezelésére szolgál, másodsorban pedig a munkaidőd nyomon követésére. Indítása a `TaskManager.exe` fájllal történik. Ha a GitHubon újabb verzió jelenik meg, a program jelzi, és a "Frissítés" gombbal magát frissíti, majd újraindul (a Beállításokban kikapcsolható).

## Feladatok kezelése

- Ctrl+F1: Feladatkezelő főablak megnyitása
- Ctrl+F2: PIN mód bekapcsolása / kikapcsolása
- Ctrl+F3: Összes időzítő megállítása / folytatása (Időstop)

Ezek az alapértelmezett kombinációk; a Beállítások **Gyorsbillentyűk** gombjával módosíthatók vagy törölhetők, és itt a hangok némításához is megadhatsz egyet (Ctrl, Alt, Shift vagy AltGr + billentyű). A hangok a tálcaikon helyi menüjéből is némíthatók.

A főablak megnyitásakor az új feladat felvételére szolgáló mező azonnal fókuszt kap. Nyomd meg a Ctrl+F1 billentyűkombinációt, írd be az új feladatot, majd nyomj Entert.  
Ha folyamatosan látni szeretnéd a feladataidat, a Ctrl+F2 billentyűkombinációval mini nézetet jeleníthetsz meg. Újabb Ctrl+F2-re a nézet eltűnik.  
**Helyi menü**: Az óra melletti ikonra jobb gombbal kattintva további funkciókat érhetsz el. Javasolt bekapcsolni az **Indítás a Windows-zal** lehetőséget.

### Pin nézet

A PIN nézet egy mini, kissé áttetsző lebegő ablak, amely mindig a képernyőn marad. A Ctrl+F2 billentyűkombinációval, a főablak PIN ikonjával vagy a helyi menüből kapcsolható be.  
**Jelölőnégyzetek:** Ebben a nézetben is késznek jelölheted a feladatokat. A kész feladatok még 10 másodpercig láthatók, így marad időd ellenőrizni vagy visszavonni a jelölést; ezután a PIN és a főablak aktív listájából is eltűnnek, és az "Elkészült" nézetben találhatók meg.
**Feladat címe:** Csak annyi szöveg látszik, amennyi kifér. Kattints a címre a főablak megnyitásához és a feladat szerkesztéséhez; a teljes szöveg ki lesz jelölve.
**Jelölések:** Látható a feladat jelölőikonja, globálisan szinkronizált feladatnál pedig a felhőikon és az "ÉN" gomb.  
**Felső fülek:** Itt látható a visszaszámlálás (piros háttér = munka, kék = pihenés, szem ikon = szem pihentetése), a Fókusz mód kapcsolója és a lebegő ablak mozgatófogantyúja.

### Főablak nézet - alapértelmezett nézet

A főablak megnyitásakor az új feladat hozzáadására szolgáló mező azonnal aktív. Ha van beállított szinkronizációs fájl, mellette felhőikon található: ezzel választhatod ki, hova kerüljön az új feladat (lásd: Szinkronizált feladatok).  
A feladatokat szerkesztheted, törölheted, készre jelölheted, jelölőikonnal láthatod el (az ikonra kattintva válthatsz a jelölések között), és húzással rendezheted sorba.  
Főablakban készre jelölve a feladat az ablak bezárásáig látható marad az aktív listában; újranyitáskor már csak az "Elkészült" nézetben szerepel.
A kész feladatokat napi bontásban, a fenti "Elkészültek" gombra kattintva láthatod.  
**Ismétlődő és későbbi feladatok:** a "+ Hozzáad" melletti ↻ gombbal (a már beírt szöveget átviszi) megadhatod, melyik naptól jelenjen meg a feladat, és hogy hány naponta / hetente / havonta ismétlődjön (0 = egyszeri). A feladat a megadott napon, a munkaidő kezdetekor kerül a lista tetejére; az ismétlődő ↻ jelet kap, és amíg nincs kész, nem jön létre belőle újabb. Az ütemezést csak te látod, a belőle létrehozott (akár szinkronizált) feladat viszont sima feladat.  
**Takarítás:** az "Elkészült" nézet jobb felső sarkában megadhatod, hány napnál régebben elkészült feladatok törlődjenek (alapból 14), majd a gombbal törölheted őket. Törlés előtt a program megmutatja, hány feladat törlődik; a szinkronizált feladatok a többi felhasználónál is törlődnek. A beírt számot a program megjegyzi.

**Gyorsbillentyűk** a főablak nézetben:

- Enter: új feladat hozzáadása, ha a beviteli mező aktív.  
- Ctrl+Enter: a beírt szöveg átvitele ismétlődő / későbbi feladatnak (mint a ↻ gomb).  
- Tab / Shift+Tab: Váltás a feladatok között
- Space: A kijelölt feladat készre jelölése vagy a jelölés visszavonása
- Enter: A kijelölt feladat szerkesztése, illetve a szerkesztés befejezése és mentése
- ESC: Szerkesztés közben visszavonja a módosításokat; egyébként befejezi és menti a szerkesztést
- Delete: Kijelölt feladat törlése (megerősítést kér)
- Dupla kattintás egy feladatra: az adott feladat szerkesztése.

### Szinkronizált feladatok

A beállításokban több szinkronizációs fájl is felvehető (**+ Új fájl hozzáadása vagy létrehozása**), soronként rövid névvel (alapból a sorszáma, átírható). A beállítások azonnal mentődnek; egyedül a munkaidő kezdetének módosítását kell a **Mentés** gombbal jóváhagyni (az OK gomb ilyenkor kiemelt "Mentés" gombra vált). A fájlválasztóban meglévő fájlt is kiválaszthatsz, vagy új fájlnevet is megadhatsz: az új fájl azonnal létrejön. A ✕ gomb csak a listából veszi ki a fájlt, magát a fájlt nem törli.  
Első indításkor, ha a felhasználói mappában van `OneDrive - Siemens AG` mappa, a program előre felveszi a `TaskManager\tasks.json` fájlt; ha nincs, nem állít be szinkronizációs fájlt. A program nem tölti fel a fájlt a hálózatra: annak megosztott helyen kell lennie (például OneDrive- vagy Google Drive-mappában), a szinkronizálást pedig a tárhelyszolgáltató alkalmazása végzi.  

**Hova kerül az új feladat?**

- Ha nincs szinkronizációs fájl, a felhőikon nem is látszik, és minden feladat helyi.
- Egy fájlnál a felhőikon ki-be kapcsolja a szinkronizálást a megadott fájlba.
- Több fájlnál a felhőikonra kattintva pipálhatod ki a célfájlokat a beállításokba megadott név alapján; az ikonon lévő szám mutatja, hány fájlba kerül a feladat.
- A Beállításokban az **A.** (alapértelmezett) oszlopban kijelölt fájlok előre ki vannak választva, és feladat felvétele után erre áll vissza a kijelölés. Ha egy sincs, akkor a feladat helyi lesz.

A szinkronizált feladatok speciálisak:

- Felhőikont kapnak. Több fájl esetén az ikonon lévő szám mutatja, hány fájlban található meg a feladat; a tooltip felsorolja a fájlok nevét.
- A dátum a legutóbbi módosítás ideje (a pipálás és az "ÉN" is ide számít). Mellette annak a neve látszik, aki a feladatot felvette, vagy utoljára átírta a szövegét; pipálás vagy "ÉN" ezt nem változtatja meg.
- Minden módosítás (szöveg, kész, "ÉN", törlés) bekerül minden olyan fájlba, amelyben a feladat szerepel. Ha más módosít egy feladatot egy olyan fájlban, amelyre te is fel vagy iratkozva, a programod a változást a többi közös fájlba is továbbítja. Mindig a legutóbb módosított változat érvényes.
- Ha valaki más felvesz egy új feladatot, vagy átírja egy feladat szövegét, felugró ablak jelez. Az új feladatról szóló ablak kattintásig marad; a módosításról szóló kb. 10 másodperc után magától eltűnik (amíg fölötte az egér, megáll). Bárhová kattintva bezárul. Induláskor nincs külön értesítés.
- A törölt feladat nem jelenik meg többé, de a fájlban 14 napig törlésjelöléssel megmarad, hogy más gépek ne hozhassák vissza.
- Nem jelölhetők meg speciális ikonnal.
- Az "ÉN" gombbal magadra veheted a feladatot. A többiek látják, ki vette fel, és a feladatot piros szín jelzi. Egy feladatot csak egy ember vehet fel; a többiek számára zárolva lesz. Ez minden fájlban közös, így olyan nevet is láthatsz, akivel nem osztasz közös fájlt.
- A feladatok sorrendje módosítható, de csak nálad érvényes; mások nem látják.
- Később nem lehet egy meglévő feladatot másik fájlhoz adni: ehhez töröld, és vedd fel újra.

Ha egy közös fájlt valaki más programverzióval használ, felugró ablak jelzi; az újabb verzió által írt adatokat a program érintetlenül hagyja.

## Időmérés

Nem ez a program fő funkciója, de hasznos a munkaidő nyomon követésére is. A képernyő előtt töltött aktív időt méri. A mérés szünetel, ha:

- Két perce nem volt billentyű- vagy egéraktivitás.
- A rendszer zárolva van.
- Manuálisan szüneteltetted az időmérést
- Az előtérben lévő ablak címe tartalmazza a **Kizárt ablakcímek** egyikét. Ezeket a Beállításokban, vesszővel elválasztva adhatod meg (alapértelmezés: YouTube, Discord, Facebook). A kis- és nagybetű nem számít; fókusz módban a mérés ilyenkor is folytatódik.
- Bezártad a programot.
- Az időzítő "STRICT MODE"-ban van, és éppen pihenőidőt mér.

Az időmérés "munkanapot" mér. A munkaidő kezdete alapértelmezés szerint 9:15.  
Az eddigi méréseket napi és heti bontásban is megtekintheted az "Idők" oldalon.  
**Takarítás:** az "Idők" nézet jobb felső sarkában megadhatod, hány hétnél régebbi mért és kézi munkaidők törlődjenek (alapból 8); a mostani hét és az előtte lévő megadott számú hét megmarad. Törlés előtt megerősítést kér.  
**Manuális** munkaidőmegadás: Az Idők nézetben kézzel is megadhatsz munkaidőt. Kiválaszthatsz egy napot, ahol módosíthatod a korábbi kézi bejegyzést. Alapból minden naphoz ez 0 óra. A kézi megadást az áttekintő nézetek más színnel jelzik.  
**Kikapcsolás:** az Idők nézetben vagy a Beállításokban az időmérés teljesen kikapcsolható; ilyenkor a munkaidő-sáv sem látszik.  
A százalékos kijelző a 8 órás munkaidőhöz viszonyítva mutatja az eltelt aktív idő arányát. Ez törvényileg szabályozott módon 6 óra 50 perc-nél jelez 100%-ot, ugyanis a törvény egy 20 perces szünetet ír elő, valamint hogy a képernyő előtti munkavégzést óránként 10 percre szüneteltetni kell. (Fontos megjegyezni: Ez NEM munkaszünet, de a program a képernyőidőt méri, azt pedig szüneteltetni kell - így a program számára ez nem számolható mint munkaidő.)

### Időzítő: Munka/pihenés

Az időzítő funkció segít az előző részben leírt munka- és pihenőciklusok betartásában. Kattints a főablakban a stopperóra ikonra. Itt beállíthatod a munka- és pihenőidő hosszát. A Start és a Pause gombbal elindíthatod, illetve szüneteltetheted az időzítőt; a Következő gombbal pedig azonnal továbbléphetsz a következő szakaszra. A program hangjelzéssel és a PIN ablak rövid villogtatásával jelzi a váltást. Ha a pihenőidő végén a gép zárolva van, vagy két perce nincs billentyű- vagy egéraktivitás (fókusz módban csak a zárolás számít), akkor a munkaszakasz hangjelzéssel elindul, de a visszaszámlálás csak az első aktivitásnál kezdődik ("Munka (aktivitásra vár)").

STRICT MODE: Ha ezt is bepipálod, akkor az időzítő kihatással lesz a munkaidő mérésre, és automatikusan szünetelteti a munkaidő mérést a pihenőidő alatt.

20-20-20 szabály: A szem egészségének megőrzése érdekében minden 20 perc képernyőnézés után nézz 20 másodpercig 20 láb (kb. 6 méter) távolságra. A program hangjelzéssel emlékeztet, amikor ideje szünetet tartani (2 csippanás), és mikor vissza kell térni (1 csippanás). A PIN ablakban a számláló szem ikonra vált a szem-pihenő alatt.

### Fókusz mód

Fókusz mód a főablakban és a PIN ablakban is elérhető (célkereszt ikon). Bekapcsolásakor a program az inaktivitás és a kizárt alkalmazások ellenére is méri az időt, amíg a gépet le nem zárolod, vagy ki nem kapcsolod a fókusz módot. A bekapcsolt mód ikonja kék.

### Időstop (időzítők megállítása)

Egy különleges állapot, amely egyszerre megállít mindent, ami az időt számolja: a munkaidő-mérést, a munka/pihenés időzítőt és a 20-20-20 szabályt. Bekapcsolható a Ctrl+F3 billentyűkombinációval, a főablak tenyér ikonjával (a fókusz mód gombja mellett) vagy a tálca helyi menüjéből ("Időzítők megállítása" / "Időzítők folytatása"). Bekapcsolva a gomb kék hátteret kap, a munkaidő-sáv szürke és "Időstop" feliratot mutat.

Ez nem azonos a gombok egyenkénti leállításával: az időzítő és a mérés saját állapota megmarad. Az időzítő gombjai Időstop alatt is használhatók, de a visszaszámlálás csak folytatáskor indul. Folytatáskor a munkaidő-mérés újraindul (ha nem állítottad le kézzel), az időzítő onnan folytatódik, ahol megállt, a 20-20-20 szabály pedig újrakezdődik egy teljes 20 perces képernyőidővel. Az állapot nem mentődik: újraindításkor az alkalmazás normál állapotban indul.

## Erőforrásigény és fordítás

Közel 0 erőforrásigény:

- Elsődleges cél a minimális erőforrásigény biztosítása.
- Tiszta Win32 C++ (nincs keretrendszer, nincs háttérben pörgő felesleges szál vagy timer).
- Eseményvezérelt `GetMessage` üzenetciklus (0% CPU tétlen állapotban).
- Minimális memóriahasználat (0.5MB - 1.5MB)
- Elrejtéskor a rendszer automatikusan kiüríti a memóriát (`SetProcessWorkingSetSize`), így az alkalmazás minimális memóriát foglal.
- Nincsenek külső hangfájlok: az alkalmazás memóriában generálja a hangokat.

A mellékelt `build.bat` parancsfájl azonnal lefordítja az alkalmazást:

```cmd
build.bat
```

A generált futtatható fájl: `TaskManager.exe` (~670 KB méretű önálló natív exe)

## Licenc

MIT - lásd [LICENSE](LICENSE).
