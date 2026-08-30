/*
 * NPC: 9906630
 * Propósito: NPC de cambio de Estética (Hair, Hair Color, Face, Face Color)
 */
var status = 0;
var category = -1;
var mainSelection = -1;
var donorCategory = -1;
var slotSelection = -1;
var price = 10000000; // 10 Millones
var donorPrice = 50; // Costo en MaplePoints (Donor Points)

// Inventario Slots config
var SLOT_QUEST_BASE = 9999101; // Quest IDs: 9999101 (Equip), 9999102 (Use), 9999103 (Setup), 9999104 (Etc)
var SLOT_BASE_PRICE = 4000; // Primera compra: 4000 NX
var SLOT_PRICE_INCREMENT = 2000; // Incremento por compra: +2000
var SLOT_AMOUNT = 8; // Slots por compra
var SLOT_MAX = 96; // Máximo de slots
var slotNames = ["Equip", "Use", "Setup", "Etc"];
var slotTypes = [1, 2, 3, 4]; // InventoryType indices

// Función auxiliar para formatear números con comas (GraalVM no soporta regex literals)
function formatNumber(num) {
    var str = "" + num;
    var result = "";
    var count = 0;
    for (var i = str.length - 1; i >= 0; i--) {
        result = str.charAt(i) + result;
        count++;
        if (count % 3 == 0 && i > 0) {
            result = "," + result;
        }
    }
    return result;
}

// Arrays de estilos VIP normales
var hair_m = [30030, 30020, 30000, 30130, 30190, 30110, 30180, 30050, 30040, 30160, 30230, 30240, 30290, 30350, 30450];
var hair_f = [31040, 31000, 31050, 31030, 31070, 31150, 31160, 31100, 31120, 31140, 31230, 31270, 31480, 31590, 31690];
var face_m = [20000, 20001, 20002, 20003, 20004, 20005, 20006, 20007, 20008, 20012, 20014, 20031];
var face_f = [21000, 21001, 21002, 21003, 21004, 21005, 21006, 21007, 21008, 21012, 21014, 21016];

// Variables para DONOR
var hairnew = [];
var haircolor = [];
var allHairs = [30000, 30010, 30020, 30030, 30040, 30050, 30060, 30070, 30080, 30090, 30100, 30110, 30120, 30130, 30140, 30150, 30160, 30170, 30180, 30190, 30200, 30210, 30220, 30230, 30240, 30250, 30260, 30270, 30280, 30290, 30300, 30310, 30320, 30330, 30340, 30350, 30360, 30370, 30380, 30400, 30410, 30420, 30430, 30440, 30450, 30460, 30470, 30480, 30490, 30510, 30520, 30530, 30540, 30550, 30560, 30570, 30580, 30590, 30600, 30610, 30620, 30630, 30640, 30650, 30660, 30670, 30680, 30690, 30700, 30710, 30720, 30730, 30740, 30750, 30760, 30770, 30780, 30790, 30800, 30810, 30820, 30830, 30840, 30850, 30860, 30870, 30880, 30890, 30900, 30910, 30920, 30930, 30940, 30950, 30960, 30970, 30990, 31000, 31010, 31020, 31030, 31040, 31050, 31060, 31070, 31080, 31090, 31100, 31110, 31120, 31130, 31140, 31150, 31160, 31170, 31180, 31190, 31200, 31210, 31220, 31230, 31240, 31250, 31260, 31270, 31280, 31290, 31300, 31310, 31320, 31330, 31340, 31350, 31360, 31400, 31410, 31420, 31430, 31440, 31450, 31460, 31470, 31480, 31490, 31510, 31520, 31530, 31540, 31550, 31560, 31570, 31580, 31590, 31600, 31610, 31620, 31630, 31640, 31650, 31660, 31670, 31680, 31690, 31700, 31710, 31720, 31730, 31740, 31750, 31760, 31770, 31780, 31790, 31800, 31810, 31820, 31830, 31840, 31850, 31860, 31870, 31880, 31890, 31900, 31910, 31920, 31930, 31940, 31950, 31960, 31970, 31980, 31990, 32000, 32010, 32020, 32030, 32040, 32050, 32160, 32310, 32320, 32330, 32340, 32350, 32360, 32370, 32380, 32390, 32400, 32410, 32420, 32430, 32440, 32450, 32460, 32470, 32480, 32490, 32500, 32510, 32520, 32530, 32540, 32550, 32560, 32640, 33000, 33010, 33020, 33030, 33040, 33050, 33060, 33070, 33080, 33090, 33100, 33110, 33120, 33130, 33140, 33150, 33160, 33170, 33180, 33190, 33210, 33220, 33240, 33250, 33260, 33270, 33280, 33290, 33310, 33320, 33330, 33340, 33350, 33360, 33370, 33380, 33390, 33400, 33410, 33430, 33440, 33450, 33460, 33470, 33480, 33500, 33510, 33520, 33530, 33540, 33550, 33580, 33590, 33600, 33610, 33620, 33630, 33640, 33660, 33670, 33680, 33690, 33700, 33710, 33720, 33730, 33740, 33750, 33760, 33770, 33780, 33790, 33800, 33810, 33820, 33830, 33930, 33940, 33950, 33960, 33990, 34000, 34010, 34020, 34030, 34040, 34050, 34060, 34070, 34080, 34090, 34100, 34110, 34120, 34130, 34140, 34150, 34160, 34170, 34180, 34190, 34210, 34220, 34230, 34240, 34250, 34260, 34270, 34290, 34310, 34320, 34330, 34340, 34350, 34360, 34370, 34380, 34400, 34410, 34420, 34430, 34440, 34450, 34470, 34480, 34490, 34510, 34540, 34560, 34580, 34590, 34600, 34610, 34620, 34630, 34640, 34650, 34660, 34670, 34680, 34690, 34700, 34710, 34720, 34730, 34740, 34750, 34760, 34770, 34780, 34790, 34800, 34810, 34820, 34830, 34840, 34850, 34860, 34870, 34880, 34890, 34900, 34910, 34940, 34950, 34960, 34970, 34980, 35000, 35010, 35020, 35040, 35050, 35060, 35070, 35090, 35100, 35110, 35120, 35130, 35140, 35150, 35160, 35170, 35180, 35190, 35200, 35210, 35220, 35240, 35260, 35280, 35290, 35300, 35310, 35330, 35340, 35350, 35360, 35420, 35430, 35440, 35450, 35460, 35470, 35490, 35500, 35510, 35520, 35530, 35550, 35590, 35620, 35640, 35650, 35660, 35680, 35690, 35720, 35740, 35760, 35790, 35950, 35960, 35980, 35990, 36000, 36010, 36020, 36030, 36040, 36050, 36060, 36070, 36080, 36090, 36100, 36110, 36130, 36140, 36150, 36160, 36170, 36180, 36190, 36200, 36210, 36220, 36230, 36240, 36250, 36260, 36270, 36280, 36300, 36310, 36320, 36330, 36340, 36350, 36380, 36390, 36400, 36410, 36420, 36450, 36460, 36470, 36480, 36490, 36500, 36510, 36520, 36530, 36560, 36570, 36590, 36600, 36610, 36630, 36640, 36650, 36670, 36680, 36690, 36700, 36720, 36740, 36750, 36760, 36770, 36780, 36790, 36800, 36810, 36820, 36830, 36840, 36850, 36860, 36870, 36880, 36890, 36900, 36910, 36920, 36930, 36940, 36950, 36960, 36980, 37000, 37010, 37020, 37030, 37040, 37050, 37060, 37070, 37080, 37090, 37100, 37110, 37120, 37130, 37140, 37150, 37160, 37170, 37190, 37200, 37210, 37220, 37230, 37240, 37250, 37260, 37270, 37300, 37310, 37320, 37330, 37340, 37350, 37370, 37380, 37400, 37420, 37440, 37450, 37460, 37470, 37490, 37500, 37510, 37520, 37530, 37570, 37580, 37590, 37600, 37610, 37640, 37660, 37670, 37690, 37700, 37710, 37720, 37750, 37800, 37810, 37820, 37830, 37840, 37850, 37860, 37880, 37900, 37920, 37930, 37940, 37950, 37960, 37980, 37990, 38010, 38020, 38070, 38090, 38100, 38120, 38140, 38150, 38240, 38270, 38280, 38290, 38310, 38320, 38330, 38350, 38380, 38390, 38400, 38410, 38420, 38440, 38450, 38460, 38470, 38480, 38490, 38510, 38540, 38560, 38570, 38580, 38590, 38610, 38660, 38670, 38680, 38690, 38730, 38740, 38750, 38760, 38770, 38780, 38790, 38800, 38810, 38840, 38860, 38880, 38890, 38900, 38930, 39340, 40010, 40020, 40030, 40040, 40050, 40060, 40070, 40080, 40090, 40100, 40110, 40120, 40260, 40270, 40280, 40290, 40300, 40310, 40320, 40350, 40360, 40370, 40390, 40400, 40410, 40420, 40440, 40450, 40460, 40470, 40480, 40490, 40500, 40510, 40530, 40540, 40550, 40560, 40570, 40580, 40590, 40610, 40620, 40630, 40640, 40680, 40700, 40730, 40740, 40750, 40780, 40790, 40800, 40810, 40820, 40830, 40840, 40890, 40910, 41190, 41200, 41400, 41570, 41580, 41610, 41640, 41740, 41750, 41790, 41800, 41820, 41830, 41840, 41880, 41890, 41920, 42100, 43000, 43010, 43020, 43120, 43130, 43140, 43150, 43170, 43180, 43190, 43200, 43230, 43240, 43250, 43260, 43270, 43320, 43350, 43420, 43630, 43660, 43680, 43690, 43730, 43740, 43750, 43800, 43820, 43830, 43870, 43880, 43890, 43900, 43910, 44000, 44010, 44030, 44090, 44120, 44180, 44190, 44310, 44320, 44330, 44340, 44360, 44370, 44400, 44410, 44420, 44430, 44440, 44450, 44460, 44470, 44480, 44500, 44510, 44520, 44590, 44600, 44610, 44650, 44790, 44800, 44810, 44820, 44830, 44850, 44880, 44900, 44910, 44920, 44930, 44940, 44950, 44980, 45000, 45020, 45030, 45050, 45060, 45070, 45080, 45090, 45100, 45110, 45120, 45130, 45150, 45160, 45220, 45230, 45280, 45310, 45320, 45340, 45360, 45370, 45430, 45440, 45450, 45460, 45470, 45480, 45490, 45510, 45520, 45570, 45620, 45640, 45680, 45740, 45760, 45770, 45780, 45840, 45850, 45860, 45870, 45880, 45900, 46010, 46020, 46060, 46070, 46080, 46110, 46160, 46170, 46180, 46210, 46220, 46230, 46240, 46340, 46370, 46380, 46390, 46400, 46410, 46420, 46430, 46440, 46450, 46460, 46480, 46490, 46500, 46510, 46610, 46640, 46680, 46710, 46760, 46780, 46800, 46810, 46820, 46830, 46840, 46850, 46860, 46870, 46880, 46890, 46900, 46910, 46920, 46930, 46940, 46950, 46960, 46970, 46980, 47030, 47040, 47060, 47070, 47090, 47110, 47150, 47160, 47270, 47280, 47290, 47300, 47310, 47320, 47340, 47350, 47360, 47370, 47380, 47390, 47400, 47410, 47420, 47450, 47460, 47520, 47530, 47540, 47550, 47560, 47570, 47590, 47600, 47610, 47620, 47630, 47640, 47650, 47660, 47670, 47680, 47690, 47700, 47710, 47740, 47750, 47800, 47810, 47830, 47870, 47910, 47930, 47980, 47990, 48000, 48040, 48060, 48070, 48080, 48140, 48180, 48200, 48340, 48360, 48410, 48430, 48440, 48470, 48480, 48520, 48530, 48540, 48550, 48560, 48570, 48610, 48620, 48630, 48710, 48740, 48790, 48810, 48890, 48900, 48920, 48930, 60000, 60020, 60060, 60070, 60080, 60100, 60110, 60120, 60180, 60190, 60200, 60210, 60220, 60230, 60240, 60250, 60260, 60270, 60280, 60290, 60300, 60310, 60320, 60330, 60340, 60350, 60360, 60370, 60380, 60390, 60400, 60420, 60430, 60480, 60490, 60500, 60510, 60520, 60530, 60540, 60550, 60680, 60920, 60930, 61020, 61030, 61050, 61060, 61070, 61080, 61090, 61100, 61110, 61120, 61130, 61150, 61180, 61190, 61200, 61210, 61220, 61230, 61240, 61250, 61260, 61270, 61280, 61290, 61300, 61310, 61320, 61330, 61340, 61350, 61360, 61370, 61380, 61390, 61400, 61410, 61420, 61430, 61440, 61450, 61460, 61470, 61480, 61490, 61500, 61510, 61530, 61550, 61560, 61570, 61650, 61660, 61700, 61760, 61770, 64010, 64180, 64190, 64230, 64270, 64280, 64300, 64350, 64360, 64410, 64420, 64450, 64460];
var style_list;

function pushIfItemExists(array, itemid) {
    if ((itemid = cm.getCosmeticItem(itemid)) != -1 && !cm.isCosmeticEquipped(itemid)) {
        array.push(itemid);
    }
}

function start() {
    status = -1;
    category = -1;
    mainSelection = -1;
    donorCategory = -1;
    slotSelection = -1;
    hairnew = [];
    haircolor = [];
    action(1, 0, 0);
}

function action(mode, type, selection) {
    if (mode == -1 || mode == 0) {
        cm.dispose();
        return;
    }
    
    if (mode == 1) {
        status++;
    }

    if (status == 0) {
        var text = "¡Hola! Soy el estilista de #bEllinMS#k. \r\n¿Qué te gustaría hacer hoy?\r\n\r\n";
        text += "#L0##bCambiar mi estilo#k#l\r\n";
        text += "#L1##dReclamar Premios Gratis#k#l\r\n";
        text += "#L2##rCambiar mi estilo (DONOR)#k#l\r\n";
        text += "#L3##gComprar Inventario Slots#k#l\r\n";
        cm.sendSimple(text);
        
    } else if (status == 1) {
        mainSelection = selection;
        
        if (mainSelection == 0) { // Estilos
            var text = "Cualquier cambio estético cuesta #r10,000,000 Mesos (10M)#k.\r\n¿Qué te gustaría cambiar?\r\n\r\n";
            text += "#L0##bCambiar estilo de Cabello#k#l\r\n";
            text += "#L1##dTeñir el Cabello (Color)#k#l\r\n";
            text += "#L2##rCambiar Ojos/Rostro (Face)#k#l\r\n";
            text += "#L3##gCambiar color de Ojos#k#l\r\n";
            cm.sendSimple(text);
        } else if (mainSelection == 1) { // Premios
            var text = "¿Qué premio gratuito deseas reclamar?\r\n\r\n";
            text += "#L0##bReclamar Smegas Gratis (x50)#k#l\r\n";
            cm.sendSimple(text);
        } else if (mainSelection == 2) { // DONOR
            var text = "¡Bienvenido a la sección VIP Donor! Cualquier cambio aquí cuesta #r" + donorPrice + " Donor Points#k.\r\n¿Qué te gustaría cambiar?\r\n\r\n";
            text += "#L0#Hair Styles (Part 1)#l\r\n";
            text += "#L1#Hair Styles (Part 2)#l\r\n";
            text += "#L2#Hair Styles (Part 3)#l\r\n";
            text += "#L3#Hair Styles (Part 4)#l\r\n";
            text += "#L4#Hair Styles (Part 5)#l\r\n";
            text += "#L5#Hair Styles (Part 6)#l\r\n";
            text += "#L6#Hair Color#l\r\n";
            cm.sendSimple(text);
        } else if (mainSelection == 3) { // Inventario Slots
            var text = "#e#b[ Comprar Inventario Slots ]#k#n\r\n\r\n";
            text += "Puedes expandir tu inventario en #b+" + SLOT_AMOUNT + " slots#k por pestaña.\r\nEl precio aumenta con cada compra. Máximo #r" + SLOT_MAX + " slots#k por pestaña.\r\n\r\n";
            
            for (var i = 0; i < slotTypes.length; i++) {
                var currentSlots = cm.getPlayer().getSlots(slotTypes[i]);
                var purchaseCount = getSlotPurchaseCount(slotTypes[i]);
                var nextPrice = SLOT_BASE_PRICE + (purchaseCount * SLOT_PRICE_INCREMENT);
                
                if (currentSlots >= SLOT_MAX) {
                    text += "#L" + i + "##r[MÁXIMO] " + slotNames[i] + " Slots (" + currentSlots + "/" + SLOT_MAX + ")#k#l\r\n";
                } else {
                    text += "#L" + i + "##b" + slotNames[i] + " Slots +" + SLOT_AMOUNT + "#k (Actual: " + currentSlots + "/" + SLOT_MAX + ") - #r" + formatNumber(nextPrice) + " NX#k#l\r\n";
                }
            }
            
            cm.sendSimple(text);
        }
        
    } else if (status == 2) {
        if (mainSelection == 0) { // Estilos -> Send Style
            category = selection;
            
            if (cm.getMeso() < price) {
                cm.sendOk("Lo siento, necesitas al menos #r10,000,000 Mesos#k para cualquier cambio de look.");
                cm.dispose();
                return;
            }

            if (category == 0) { // Cabello
                style_list = (cm.getPlayer().getGender() == 0) ? hair_m : hair_f;
                cm.sendStyle("Elige el estilo de cabello que más te guste:", style_list);
            } else if (category == 1) { // Color de Cabello
                var currentHair = cm.getPlayer().getHair();
                var baseHair = Math.floor(currentHair / 10) * 10;
                style_list = [];
                for (var i = 0; i < 8; i++) {
                    style_list.push(baseHair + i);
                }
                cm.sendStyle("Elige el color que deseas para tu cabello actual:", style_list);
            } else if (category == 2) { // Face
                style_list = (cm.getPlayer().getGender() == 0) ? face_m : face_f;
                cm.sendStyle("Elige el estilo de rostro que más te guste:", style_list);
            } else if (category == 3) { // Color de Face
                var currentFace = cm.getPlayer().getFace();
                var faceType = currentFace % 100; // Extrae el ID base de la cara (0 a 99)
                var genderFaceBase = (cm.getPlayer().getGender() == 0) ? 20000 : 21000;
                
                style_list = [];
                for (var i = 0; i < 8; i++) {
                    style_list.push(genderFaceBase + (i * 100) + faceType);
                }
                cm.sendStyle("Elige el color que deseas para tus ojos:", style_list);
            }
        } else if (mainSelection == 1) { // Premios -> Reclamar
            if (selection == 0) { // Smegas
                var smegaId = 5390001;
                var currentSmegas = cm.getItemQuantity(smegaId);
                if (currentSmegas <= 5) {
                    if (cm.canHold(smegaId, 50)) {
                        cm.gainItem(smegaId, 50);
                        cm.sendOk("¡Aquí tienes tus 50 Smegas gratis! Úsalos sabiamente.");
                    } else {
                        cm.sendOk("No tienes espacio suficiente en tu inventario Cash.");
                    }
                } else {
                    cm.sendOk("Ya tienes suficientes Smegas. Solo puedes reclamar más si tienes 5 o menos en tu inventario.");
                }
            }
            cm.dispose();
        } else if (mainSelection == 2) { // DONOR -> Send Style
            donorCategory = selection;
            hairnew = [];
            haircolor = [];
            
            if (donorCategory >= 0 && donorCategory <= 5) { // Part 1 to 6
                var chunk = Math.ceil(allHairs.length / 6);
                var selectedHair = allHairs.slice(donorCategory * chunk, (donorCategory + 1) * chunk);
                for each(var i in selectedHair) {
                    pushIfItemExists(hairnew, i);
                }
                cm.sendStyle("Elige un estilo:", hairnew);
            } else if (donorCategory == 6) { // Color
                var baseHair = parseInt(cm.getPlayer().getHair() / 10) * 10;
                for (var k = 0; k < 8; k++) {
                    pushIfItemExists(haircolor, baseHair + k);
                }
                cm.sendStyle("Elige un color:", haircolor);
            } else {
                cm.dispose();
            }
        } else if (mainSelection == 3) { // Inventario Slots -> Confirmar compra
            slotSelection = selection;
            var invType = slotTypes[slotSelection];
            var currentSlots = cm.getPlayer().getSlots(invType);
            
            if (currentSlots >= SLOT_MAX) {
                cm.sendOk("Ya tienes el máximo de #r" + SLOT_MAX + " slots#k en tu inventario de #b" + slotNames[slotSelection] + "#k. No puedes comprar más.");
                cm.dispose();
                return;
            }
            
            var purchaseCount = getSlotPurchaseCount(invType);
            var nxCost = SLOT_BASE_PRICE + (purchaseCount * SLOT_PRICE_INCREMENT);
            var nxAvailable = cm.getPlayer().getCashShop().getCash(1);
            
            if (nxAvailable < nxCost) {
                cm.sendOk("No tienes suficiente NX. Necesitas #r" + formatNumber(nxCost) + " NX#k pero solo tienes #b" + formatNumber(nxAvailable) + " NX#k.");
                cm.dispose();
                return;
            }
            
            var text = "¿Estás seguro que deseas comprar #b+" + SLOT_AMOUNT + " " + slotNames[slotSelection] + " Slots#k?\r\n\r\n";
            text += "Slots actuales: #b" + currentSlots + "#k → #g" + (currentSlots + SLOT_AMOUNT) + "#k\r\n";
            text += "Costo: #r" + formatNumber(nxCost) + " NX#k\r\n";
            text += "Tu NX: #b" + formatNumber(nxAvailable) + " NX#k";
            
            cm.sendYesNo(text);
        }
        
    } else if (status == 3) {
        if (mainSelection == 0) { // Estilos -> Aplicar
            if (cm.getMeso() >= price) {
                cm.gainMeso(-price);
                
                if (category == 0 || category == 1) {
                    cm.setHair(style_list[selection]);
                } else if (category == 2 || category == 3) {
                    cm.setFace(style_list[selection]);
                }
                
                cm.sendOk("¡Disfruta tu nuevo estilo VIP!");
            } else {
                cm.sendOk("Lo siento, necesitas al menos #r10,000,000 Mesos#k para realizar este cambio.");
            }
            cm.dispose();
        } else if (mainSelection == 2) { // DONOR -> Aplicar
            var donorPoints = cm.getPlayer().getCashShop().getCash(2);
            if (donorPoints >= donorPrice) {
                cm.getPlayer().getCashShop().gainCash(2, -donorPrice);
                
                if (donorCategory >= 0 && donorCategory <= 5) {
                    cm.setHair(hairnew[selection]);
                    cm.sendOk("¡Disfruta tu nuevo estilo de DONOR!");
                } else if (donorCategory == 6) {
                    cm.setHair(haircolor[selection]);
                    cm.sendOk("¡Disfruta tu nuevo color de DONOR!");
                }
            } else {
                cm.sendOk("Lo siento, necesitas al menos #r" + donorPrice + " Donor Points#k para realizar este cambio.");
            }
            cm.dispose();
        } else if (mainSelection == 3) { // Inventario Slots -> Ejecutar compra
            var invType = slotTypes[slotSelection];
            var currentSlots = cm.getPlayer().getSlots(invType);
            var purchaseCount = getSlotPurchaseCount(invType);
            var nxCost = SLOT_BASE_PRICE + (purchaseCount * SLOT_PRICE_INCREMENT);
            var nxAvailable = cm.getPlayer().getCashShop().getCash(1);
            
            // Doble verificación de seguridad
            if (currentSlots >= SLOT_MAX) {
                cm.sendOk("Ya alcanzaste el máximo de slots.");
                cm.dispose();
                return;
            }
            if (nxAvailable < nxCost) {
                cm.sendOk("No tienes suficiente NX.");
                cm.dispose();
                return;
            }
            if (!cm.getPlayer().canGainSlots(invType, SLOT_AMOUNT)) {
                cm.sendOk("No puedes exceder el máximo de #r" + SLOT_MAX + " slots#k.");
                cm.dispose();
                return;
            }
            
            // Cobrar NX y dar slots
            cm.getPlayer().getCashShop().gainCash(1, -nxCost);
            cm.getPlayer().gainSlots(invType, SLOT_AMOUNT);
            
            // Registrar la compra
            incrementSlotPurchaseCount(invType);
            
            var newSlots = cm.getPlayer().getSlots(invType);
            cm.sendOk("¡Compra exitosa! Tu inventario de #b" + slotNames[slotSelection] + "#k ahora tiene #g" + newSlots + " slots#k.\r\n\r\nVuelve cuando quieras expandir más.");
            cm.dispose();
        }
    }
}

// Funciones auxiliares para el sistema de Inventario Slots
function getSlotPurchaseCount(invType) {
    var qId = SLOT_QUEST_BASE + invType - 1; // 9999101=Equip, 9999102=Use, 9999103=Setup, 9999104=Etc
    var record = cm.getQuestRecord(qId);
    if (record != null && record.getCustomData() != null && ("" + record.getCustomData()) != "") {
        return parseInt("" + record.getCustomData());
    }
    return 0;
}

function incrementSlotPurchaseCount(invType) {
    var qId = SLOT_QUEST_BASE + invType - 1;
    var count = getSlotPurchaseCount(invType) + 1;
    var record = cm.getQuestRecord(qId);
    if (record != null) {
        record.setCustomData("" + count);
    }
}
