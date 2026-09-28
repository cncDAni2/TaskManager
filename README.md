# Ultra-könnyű Windows Feladatkezelő (TaskManager)

Egy minimális erőforrásigényű (közel 0% CPU, ~2-4 MB RAM aktív állapotban, háttérbe rejtve < 1.5 MB RAM) natív C++ Win32 feladatkezelő alkalmazás.

---

## Főbb Jellemzők

1. **Rendszertálca (System Tray) és Gyorsbillentyűk**:
   - A tálca jobb alsó sarkában az óra mellett fut egy elegáns ikonnal.
   - Bal kattintásra vagy a **`Ctrl + F1`** globális gyorsbillentyűre felugrik a képernyő jobb alsó sarkában (a tálca felett).
   - A **`Ctrl + F2`** globális gyorsbillentyű kétirányú (toggle):
     - Ha az ablak nyitva van: kikapcsolja a PIN módot és bezárja az ablakot a tálcára.
     - Ha az ablak rejtve van: bekapcsolja a PIN módot és azonnal megnyitja a kis lebegő PIN ablakot a jobb alsó sarokban.
   - Jobb kattintással elérhető a helyi menü: *Megnyitás*, *Elkészült feladatok*, *Kilépés*.
   - Az **`Esc`** billentyű vagy a fejléc **`✕`** gombja azonnal visszacsukja a tálcára.

2. **Rögzített (PIN) mód**:
   - Indításkor az alkalmazás **automatikusan bekapcsolt PIN móddal és közvetlenül a kis lebegő ablak megjelenítésével** indul.
   - A fejléc jobb oldalán az `✕` mellett található a **Pin** (gombostű) gomb, illetve a jobb klikkes helyi menüben is elérhető a **PIN mód** opció.
   - Bekapcsolt állapotban fókuszvesztéskor az ablak nem záródik be, hanem egy lebegő, minimalista módba vált:
     - 80%-os áttetszőség (`opacity: 80%`), fél szélességű lebegő ablak.
     - Magassága automatikusan igazodik a jelenleg aktív feladatok számához.
     - Csak a jelenlegi aktív feladatok listázódnak; itt közvetlenül pipálhatóak / befejezhetőek.
     - Bármely feladat kártyájára kattintva a teljes méretű ablak azonnal megnyílik és egyből szerkesztő (inline edit) módba lép a kiválasztott feladatra.
     - **Munkaidő kijelző csík**: az ablak legalján egy 3px magas sáv mutatja a munkaidő százalékos előrehaladását (ugyanúgy a 8 órás cél alapján arányos szélességgel, mint a normál módban, szöveg nélkül). A kitöltés **zöld**, amikor a számláló aktívan számol, és **piros**, amikor szünetel.
   - Kilépés a PIN módból és bezárás:
     - A **`Ctrl + F2`** gyorsbillentyűvel (kikapcsolja a PIN módot és bezárja az ablakot)
     - A teljes ablakon az `✕` gombra kattintva
     - A teljes ablakon újra a PIN ikonra kattintva
     - A jobb klikkes helyi menüből a "PIN mód" pipáját kivéve.

3. **Aktív feladatok kezelése**:
   - **Új feladat hozzáadása**: szövegmező + `Enter` vagy `+ Hozzáad` gomb.
   - **Globális / Szinkronizált feladat (OneDrive)**: a `+ Hozzáad` gomb melletti felhő (`☁`) toggle gombbal bekapcsolható (kék hátterűvé válik). Mentés után automatikusan visszaáll normál helyi feladatra.
   - **Szerkesztés**: ✎ gombra kattintva vagy a feladatra duplán kattintva felugró szerkesztő ablak.
   - **Törlés**: 🗑 kuka ikonra kattintva.
   - **Elkészültnek jelölés**: a bal oldali jelölőnégyzetre `[ ]` kattintva a feladat zöld pipát kap és átkerül az elkészült feladatok közé a pontos elkészülési időbélyeggel.

4. **Perzisztens adattárolás & Felhő-szinkronizáció**:
   - **Helyi feladatok**: az alkalmazás mellett lévő `tasks.json` fájlba mentődnek (munkaidővel és beállításokkal).
   - **Szinkronizált feladatok**: kizárólag feladatokat és a hozzáadó nevét tartalmazó megosztott JSON fájlba mentődnek (alapértelmezés: `C:\Users\<user>\OneDrive - Siemens AG\TaskManager\tasks.json`).
   - **Fájlválasztó**: a fejlécben a bezárás (`✕`) mellett lévő mappa-felhő ikonra kattintva a szabványos Windows fájlkiválasztóval bármikor megváltoztatható a szinkronizációs fájl helye.
   - **Megjelenítés**: a szinkronizált feladatok előtt kis felhő (`☁`) ikon látható a fő listában, a PIN (mini) ablakban és az előzményeknél is, a meta sorban pedig megjelenik a rögzítő neve (pl. `Burunkai, Dániel`).

5. **Elkészült feladatok nézet**:
   - Az ablak jobb felső sarkában lévő **`Elkészült feladatok (N) →`** gombbal érhető el.
   - **Alapértelmezett szűrés**: csak az **előző nap 9:30 óta** elkészült feladatokat mutatja.
   - **`MIND (napi bontás)`** fül: megjeleníti a teljes előzményt, naponkénti csoportosításban és fejlécbontásban (`Ma`, `Tegnap`, stb.).
   - Az elkészült feladatok mellett:
     - **Kuka ikon (`🗑`)**: végleges törlés.
     - **Jelölőnégyzet (`[✓]`)**: visszakattintva a feladat újra aktívvá válik! (Szerkesztési lehetőség itt a kérésnek megfelelően nincs, csak törlés és újra-aktiválás).

6. **Munkaidő nézet**:
   - Az **`Idők`** gombbal a napi és heti munkaidő-előzmény jelenik meg.
   - A **`Manuális`** gombbal dátumonként szerkeszthető a kézi idő órában; a kiválasztott nap meglévő értéke előtöltődik, a `0` törli azt.
   - A mért és manuálisan hozzáadott idő eltérő színű csíkszakaszt kap; a részletek a csík fölötti eszköztippben láthatók.

7. **Közel 0 erőforrásigény**:
   - Tiszta Win32 C++ (nincs keretrendszer, nincs háttérben pörgő felesleges szál vagy timer).
   - Eseményvezérelt `GetMessage` üzenetciklus (0.00% CPU tétlen állapotban).
   - Elrejtéskor a rendszer automatikusan kiüríti a memóriát (`SetProcessWorkingSetSize`), így az alkalmazás minimális memóriát foglal.

---

## Fordítás

A mellékelt `build.bat` parancsfájl azonnal lefordítja az alkalmazást:
```cmd
build.bat
```
A generált futtatható fájl: `TaskManager.exe` (~350 KB méretű önálló natív exe). A fordítás köztes fájljai a `dist/` mappába kerülnek.
