"""The shipped INI and the eleven translation files for HUD Position Manager."""
import ast, os, re

R = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OR_GENDIST = os.path.join(os.path.dirname(R), "HUDPositionManagerOR", "tools", "gen-dist.py")
os.makedirs(R + r"\dist\SKSE\Plugins", exist_ok=True)
os.makedirs(R + r"\dist\Interface\Translations", exist_ok=True)


def element_rows():
    """Every row of the element table: (key, name, menu, moveWith, stretch, fades, bar). A row may span lines."""
    src = open(R + r"\source\Elements.cpp", encoding="utf-8").read()
    rows = []
    for m in re.finditer(r'^\s*\{ "([A-Za-z]+)", "([^"]+)", \{', src, re.M):
        i, depth = m.end(), 1
        while depth:   # the parts list's closing brace
            depth += {"{": 1, "}": -1}.get(src[i], 0)
            i += 1
        j, depth = i, 1
        while depth:   # the row's closing brace
            depth += {"{": 1, "}": -1}.get(src[j], 0)
            j += 1
        rest = [t.strip() for t in src[i:j - 1].split(",") if t.strip()]
        rest += ["nullptr", "nullptr", "false", "false", "false"][len(rest):]
        q = lambda t: None if t == "nullptr" else t.strip('"')
        rows.append((m.group(1), m.group(2), q(rest[0]), q(rest[1]), rest[2] == "true", rest[3] == "true", rest[4] == "true"))
    return rows


rows = element_rows()
_count = len(re.findall(r'^\s*\{ "[A-Za-z]+", "', open(R + r"\source\Elements.cpp", encoding="utf-8").read(), re.M))
assert len(rows) == _count, f"parsed {len(rows)} element rows of {_count} in Elements.cpp - fix the parser"
elements = [(k, n) for k, n, *_ in rows]

ini = ["; HUD Position Manager - its settings page in the Apocrypha Menu Framework edits this file for you.",
       "; fX / fY move an element: a percentage of the screen (right / down positive), added to where the HUD puts it.",
       "; fScale is times the size the HUD gives it, about the element's centre; fLength / fHeight stretch one side on top of it.",
       "; bHide hides the element. iShow: 0 always (as the game decides), 1 only in combat, 2 only out of combat.",
       "; bAlwaysVisible (the elements the game fades on its own): 1 keeps it shown while you play.",
       "; sMoveWith names another element (Health, Stamina, ...) whose offset this one also takes: a widget beside a bar follows it.",
       "", "[General]", "; 1 = apply the layout below, 0 = every element back where the HUD puts it", "bEnabled=1",
       "; 1 = Magicka and Stamina move with Health (the three bars as one block)", "bLinkBars=1",
       "; 1 = widgets a UI places around the bars (Norden UI: STB Widgets, TrueHUD's bars) move with the bars", "bLinkWidgets=1",
       "; 1 = the bars stay shown while you play instead of fading out when full", "bAlwaysVisible=0",
       "; 1 = Free placement: the move sliders go past the screen's edges", "bUnlocked=0",
       "; Log level: 0 trace, 1 debug, 2 info, 3 warn, 4 error. Raise to 0 for a bug report.", "uLogLevel=2",
       "", "[Group]", "; the Combined widgets tab: these elements (comma-separated keys) move as one by fX / fY", "sMembers=", "fX=0", "fY=0"]
for key, _name, _menu, follow, stretch, fades, _bar in rows:
    ini += ["", f"[{key}]", "fX=0", "fY=0", "fScale=1", "fLength=1", "fHeight=1", "bHide=0", "iShow=0"]
    if fades:
        ini += ["bAlwaysVisible=0"]
    ini += [f"sMoveWith={follow or ''}"]
open(R + r"\dist\SKSE\Plugins\HUDPositionManager.ini", "w", encoding="utf-8", newline="").write("\r\n".join(ini) + "\r\n")

# key -> [english, japanese, korean, chinese, russian, german, french, spanish, italian, polish, czech]
T = {
    "HPM_Intro": ["Move, resize or hide each part of the HUD. Changes show in the HUD at once and are saved automatically.",
                  "HUDの各要素を移動・拡大縮小・非表示にできます。変更はすぐにHUDへ反映され、自動で保存されます。",
                  "HUD의 각 요소를 이동하거나 크기를 바꾸거나 숨깁니다. 변경 사항은 즉시 HUD에 반영되고 자동으로 저장됩니다.",
                  "移动、缩放或隐藏 HUD 的每个部分。更改会立即显示在 HUD 中，并自动保存。",
                  "Перемещайте, масштабируйте или скрывайте любую часть интерфейса. Изменения сразу видны в интерфейсе и сохраняются автоматически.",
                  "Verschiebe, skaliere oder verstecke jeden Teil des HUD. Änderungen erscheinen sofort im HUD und werden automatisch gespeichert.",
                  "Déplacez, redimensionnez ou masquez chaque partie de l'ATH. Les changements s'affichent aussitôt et sont enregistrés automatiquement.",
                  "Mueve, cambia de tamaño u oculta cada parte del HUD. Los cambios se ven al instante en el HUD y se guardan automáticamente.",
                  "Sposta, ridimensiona o nascondi ogni parte dell'HUD. Le modifiche appaiono subito nell'HUD e vengono salvate automaticamente.",
                  "Przesuwaj, skaluj lub ukrywaj każdy element HUD. Zmiany widać od razu, a zapis jest automatyczny.",
                  "Přesouvejte, měňte velikost nebo skryjte každou část HUD. Změny se v HUD projeví hned a ukládají se automaticky."],
    "HPM_Enabled": ["Apply my layout", "レイアウトを適用", "내 배치 적용", "应用我的布局", "Применять мою раскладку", "Mein Layout anwenden",
                    "Appliquer ma disposition", "Aplicar mi diseño", "Applica il mio layout", "Stosuj mój układ", "Použít mé rozvržení"],
    "HPM_EnabledHint": ["off: every element back where the HUD puts it", "オフ: すべての要素をHUDの既定位置に戻す", "끄기: 모든 요소가 HUD 기본 위치로 돌아갑니다",
                        "关闭：所有元素回到 HUD 默认位置", "выкл.: все элементы на своих обычных местах", "aus: jedes Element wieder dort, wo das HUD es hinsetzt",
                        "désactivé : chaque élément revient à sa place d'origine", "desactivado: cada elemento vuelve a donde lo pone el HUD",
                        "disattivato: ogni elemento torna dove lo mette l'HUD", "wyłączone: każdy element wraca na swoje miejsce",
                        "vypnuto: každý prvek se vrátí tam, kam ho dává HUD"],
    "HPM_NoHud": ["The HUD has not been shown yet - load a game to see this element.", "HUDはまだ表示されていません。ゲームをロードするとこの要素が表示されます。",
                  "아직 HUD가 표시되지 않았습니다. 게임을 불러오면 이 요소를 볼 수 있습니다.", "HUD 尚未显示——载入游戏后即可看到此元素。",
                  "Интерфейс ещё не показан - загрузите игру, чтобы увидеть этот элемент.", "Das HUD wurde noch nicht angezeigt - lade ein Spiel, um dieses Element zu sehen.",
                  "L'ATH n'est pas encore affiché - chargez une partie pour voir cet élément.", "El HUD aún no se ha mostrado; carga una partida para ver este elemento.",
                  "L'HUD non è ancora stato mostrato: carica una partita per vedere questo elemento.", "HUD nie był jeszcze wyświetlony - wczytaj grę, aby zobaczyć ten element.",
                  "HUD se zatím nezobrazil - načtěte hru, abyste tento prvek viděli."],
    "HPM_NotFound": ["Not found in your HUD: the HUD you use may not have this element, or names it differently. Its settings are kept but do nothing.",
                     "お使いのHUDに見つかりません。このHUDにはこの要素がないか、別の名前になっている可能性があります。設定は保持されますが効果はありません。",
                     "사용 중인 HUD에서 찾을 수 없습니다. 이 요소가 없거나 다른 이름일 수 있습니다. 설정은 유지되지만 적용되지 않습니다.",
                     "在你的 HUD 中未找到：你使用的 HUD 可能没有此元素，或名称不同。其设置会保留但不起作用。",
                     "Не найден в вашем интерфейсе: в нём может не быть этого элемента или он называется иначе. Настройки сохраняются, но ни на что не влияют.",
                     "In deinem HUD nicht gefunden: Dein HUD hat dieses Element vielleicht nicht oder nennt es anders. Die Einstellungen bleiben erhalten, bewirken aber nichts.",
                     "Introuvable dans votre ATH : il n'a peut-être pas cet élément ou le nomme autrement. Ses réglages sont conservés mais sans effet.",
                     "No se encuentra en tu HUD: puede que no tenga este elemento o que lo llame de otra forma. Sus ajustes se conservan pero no hacen nada.",
                     "Non trovato nel tuo HUD: potrebbe non avere questo elemento o chiamarlo in altro modo. Le impostazioni restano ma non hanno effetto.",
                     "Nie znaleziono w twoim HUD: może nie mieć tego elementu lub nazywa go inaczej. Ustawienia zostają zachowane, ale nic nie robią.",
                     "Ve vašem HUD nenalezeno: nemusí tento prvek mít nebo ho pojmenovává jinak. Nastavení zůstává, ale nemá účinek."],
    "HPM_Found": ["In your HUD - changes show at once.", "HUDにあります。変更はすぐに反映されます。", "HUD에 있습니다. 변경 사항이 즉시 반영됩니다.",
                  "存在于你的 HUD 中——更改立即生效。", "Есть в вашем интерфейсе - изменения видны сразу.", "In deinem HUD - Änderungen erscheinen sofort.",
                  "Présent dans votre ATH - les changements s'affichent aussitôt.", "Está en tu HUD: los cambios se ven al instante.",
                  "Presente nel tuo HUD: le modifiche appaiono subito.", "Jest w twoim HUD - zmiany widać od razu.", "Je ve vašem HUD - změny se projeví hned."],
    "HPM_MoveX": ["Move left / right", "左右に移動", "좌우 이동", "左右移动", "Сдвиг влево / вправо", "Nach links / rechts",
                  "Déplacer à gauche / droite", "Mover izquierda / derecha", "Sposta a sinistra / destra", "Przesuń w lewo / prawo", "Posun vlevo / vpravo"],
    "HPM_MoveY": ["Move up / down", "上下に移動", "상하 이동", "上下移动", "Сдвиг вверх / вниз", "Nach oben / unten",
                  "Déplacer en haut / bas", "Mover arriba / abajo", "Sposta su / giù", "Przesuń w górę / dół", "Posun nahoru / dolů"],
    "HPM_Size": ["Size", "サイズ", "크기", "大小", "Размер", "Größe", "Taille", "Tamaño", "Dimensione", "Rozmiar", "Velikost"],
    "HPM_Hide": ["Hide", "非表示", "숨기기", "隐藏", "Скрыть", "Ausblenden", "Masquer", "Ocultar", "Nascondi", "Ukryj", "Skrýt"],
    "HPM_ResetOne": ["Reset this element", "この要素をリセット", "이 요소 초기화", "重置此元素", "Сбросить этот элемент", "Dieses Element zurücksetzen",
                     "Réinitialiser cet élément", "Restablecer este elemento", "Ripristina questo elemento", "Resetuj ten element", "Obnovit tento prvek"],
    "HPM_ResetAll": ["Reset every element", "すべての要素をリセット", "모든 요소 초기화", "重置所有元素", "Сбросить все элементы", "Alle Elemente zurücksetzen",
                     "Réinitialiser tous les éléments", "Restablecer todos los elementos", "Ripristina tutti gli elementi", "Resetuj wszystkie elementy", "Obnovit všechny prvky"],
    "HPM_El_Health": ["Health", "体力", "체력", "生命值", "Здоровье", "Gesundheit", "Santé", "Salud", "Salute", "Zdrowie", "Zdraví"],
    "HPM_El_Magicka": ["Magicka", "マジカ", "매지카", "法力值", "Магия", "Magicka", "Magie", "Magia", "Magicka", "Magia", "Magie"],
    "HPM_El_Stamina": ["Stamina", "スタミナ", "지구력", "耐力值", "Запас сил", "Ausdauer", "Vigueur", "Aguante", "Vigore", "Wytrzymałość", "Výdrž"],
    "HPM_El_LeftCharge": ["Left charge meter", "左手チャージメーター", "왼손 충전 게이지", "左手充能条", "Заряд левой руки", "Ladeanzeige links",
                          "Jauge de charge gauche", "Medidor de carga izquierdo", "Carica sinistra", "Wskaźnik naładowania - lewa", "Nabití - levá"],
    "HPM_El_RightCharge": ["Right charge meter", "右手チャージメーター", "오른손 충전 게이지", "右手充能条", "Заряд правой руки", "Ladeanzeige rechts",
                           "Jauge de charge droite", "Medidor de carga derecho", "Carica destra", "Wskaźnik naładowania - prawa", "Nabití - pravá"],
    "HPM_El_Compass": ["Compass", "コンパス", "나침반", "罗盘", "Компас", "Kompass", "Boussole", "Brújula", "Bussola", "Kompas", "Kompas"],
    "HPM_El_Crosshair": ["Crosshair", "照準", "조준점", "准星", "Прицел", "Fadenkreuz", "Réticule", "Punto de mira", "Mirino", "Celownik", "Zaměřovač"],
    "HPM_El_EnemyHealth": ["Enemy health", "敵の体力", "적 체력", "敌人生命值", "Здоровье врага", "Gegnergesundheit", "Santé de l'ennemi",
                           "Salud del enemigo", "Salute del nemico", "Zdrowie wroga", "Zdraví nepřítele"],
    "HPM_El_StealthMeter": ["Stealth meter", "隠密メーター", "은신 게이지", "潜行指示", "Индикатор скрытности", "Schleichanzeige", "Indicateur de discrétion",
                            "Indicador de sigilo", "Indicatore di furtività", "Wskaźnik skradania", "Ukazatel plížení"],
    "HPM_El_Subtitles": ["Subtitles", "字幕", "자막", "字幕", "Субтитры", "Untertitel", "Sous-titres", "Subtítulos", "Sottotitoli", "Napisy", "Titulky"],
    "HPM_El_ArrowInfo": ["Ammo count", "矢弾の数", "탄약 수", "弹药数量", "Боеприпасы", "Munitionsanzeige", "Munitions", "Munición", "Munizioni", "Amunicja", "Střelivo"],
    "HPM_El_Messages": ["Notifications", "通知", "알림", "通知", "Уведомления", "Benachrichtigungen", "Notifications", "Notificaciones", "Notifiche", "Powiadomienia", "Oznámení"],
    "HPM_El_QuestUpdate": ["Quest updates", "クエスト更新", "퀘스트 갱신", "任务更新", "Обновления заданий", "Questaktualisierungen", "Mises à jour de quête",
                           "Actualizaciones de misión", "Aggiornamenti delle missioni", "Aktualizacje zadań", "Aktualizace úkolů"],
    "HPM_El_ActivatePrompt": ["Activate prompt", "アクティベート表示", "상호작용 안내", "交互提示", "Подсказка действия", "Aktivierungshinweis",
                              "Invite d'activation", "Indicador de activar", "Prompt di attivazione", "Podpowiedź akcji", "Výzva k aktivaci"],
    "HPM_El_LocationText": ["Location name", "場所の名前", "장소 이름", "地点名称", "Название локации", "Ortsname", "Nom du lieu", "Nombre del lugar",
                            "Nome del luogo", "Nazwa miejsca", "Název místa"],
    "HPM_WidgetClosed": ["This widget is not showing: the mod that adds it is not installed, or has it switched off. Its settings are kept.",
                         "このウィジェットは表示されていません。追加するMODが入っていないか、オフになっています。設定は保持されます。",
                         "이 위젯은 표시되지 않습니다. 추가하는 모드가 설치되지 않았거나 꺼져 있습니다. 설정은 유지됩니다.",
                         "此小部件未显示：添加它的模组未安装或已关闭。其设置会保留。",
                         "Этот виджет не показан: добавляющий его мод не установлен или выключил его. Настройки сохраняются.",
                         "Dieses Widget wird nicht angezeigt: Die Mod, die es hinzufügt, ist nicht installiert oder hat es ausgeschaltet. Die Einstellungen bleiben erhalten.",
                         "Ce widget n'est pas affiché : le mod qui l'ajoute n'est pas installé ou l'a désactivé. Ses réglages sont conservés.",
                         "Este widget no se muestra: el mod que lo añade no está instalado o lo tiene desactivado. Sus ajustes se conservan.",
                         "Questo widget non è visibile: la mod che lo aggiunge non è installata o lo ha disattivato. Le impostazioni restano.",
                         "Ten widżet nie jest wyświetlany: dodający go mod nie jest zainstalowany lub go wyłączył. Ustawienia zostają zachowane.",
                         "Tento widget se nezobrazuje: mod, který ho přidává, není nainstalován nebo ho vypnul. Nastavení zůstává."],
    "HPM_MoveWith": ["Move with", "一緒に動かす", "함께 이동", "跟随移动", "Двигать вместе с", "Mitbewegen mit", "Suivre", "Mover con", "Sposta con", "Przesuwaj z", "Posouvat s"],
    "HPM_MoveWithNone": ["Nothing - on its own", "なし (単独)", "없음 - 단독", "无——独立", "Ничем - сам по себе", "Nichts - eigenständig", "Rien - indépendant",
                         "Nada - por sí solo", "Niente - da solo", "Nic - samodzielnie", "Nic - samostatně"],
    "HPM_El_CombinedCharge": ["Combined charge meters", "統合チャージメーター", "통합 충전 게이지", "合并充能条", "Общий заряд", "Kombinierte Ladeanzeige",
                              "Jauges de charge combinées", "Medidores de carga combinados", "Cariche combinate", "Połączone wskaźniki naładowania", "Společné nabití"],
    "HPM_El_ShoutMeter": ["Shout meter", "シャウトメーター", "용언 게이지", "龙吼指示条", "Шкала Криков", "Schreianzeige", "Jauge de cri", "Medidor de grito",
                          "Barra dell'urlo", "Wskaźnik okrzyku", "Ukazatel pokřiku"],
    "HPM_El_LevelUp": ["Level-up meter", "レベルアップメーター", "레벨 업 게이지", "升级指示", "Шкала уровня", "Stufenanzeige", "Jauge de niveau", "Medidor de nivel",
                       "Indicatore di livello", "Wskaźnik poziomu", "Ukazatel úrovně"],
    "HPM_El_AnimLetters": ["Word wall letters", "言葉の壁の文字", "용언 벽 글자", "龙语墙文字", "Буквы Стены слов", "Wortwand-Buchstaben", "Lettres du mur de mots",
                           "Letras del muro de palabras", "Lettere del muro delle parole", "Litery ściany słów", "Písmena zdi slov"],
    "HPM_El_Clock": ["Clock", "時計", "시계", "时钟", "Часы", "Uhr", "Horloge", "Reloj", "Orologio", "Zegar", "Hodiny"],
    "HPM_LinkBars": ["Move the three bars together", "3本のバーを一緒に動かす", "세 바를 함께 이동", "三条状态条一起移动", "Двигать три полосы вместе",
                     "Die drei Leisten gemeinsam bewegen", "Déplacer les trois barres ensemble", "Mover las tres barras juntas", "Sposta insieme le tre barre",
                     "Przesuwaj trzy paski razem", "Posouvat tři lišty společně"],
    "HPM_LinkBarsHint": ["Magicka and Stamina move with Health", "マジカとスタミナが体力と一緒に動く", "매지카와 지구력이 체력과 함께 이동", "法力和耐力随生命移动",
                         "Магия и запас сил следуют за здоровьем", "Magicka und Ausdauer folgen der Gesundheit", "La magie et la vigueur suivent la santé",
                         "La magia y el aguante siguen a la salud", "Magicka e vigore seguono la salute", "Magia i wytrzymałość podążają za zdrowiem",
                         "Magie a výdrž následují zdraví"],
    "HPM_LinkWidgets": ["Widgets around the bars move with them", "バー周りのウィジェットも一緒に動かす", "바 주변 위젯도 함께 이동", "状态条周围的小部件随之移动",
                        "Виджеты у полос двигаются вместе с ними", "Widgets um die Leisten bewegen sich mit", "Les widgets autour des barres les suivent",
                        "Los widgets alrededor de las barras las siguen", "I widget intorno alle barre le seguono", "Widżety wokół pasków przesuwają się z nimi",
                        "Widgety u lišt se posouvají s nimi"],
    "HPM_LinkWidgetsHint": ["for a UI that places widgets around the bars, like Norden UI", "Norden UIのようにバーの周りにウィジェットを置くUI向け",
                            "Norden UI처럼 바 주변에 위젯을 두는 UI용", "适用于像 Norden UI 这样在状态条周围放置小部件的界面",
                            "для интерфейсов, размещающих виджеты у полос, как Norden UI", "für ein UI, das Widgets um die Leisten legt, wie Norden UI",
                            "pour une interface qui place des widgets autour des barres, comme Norden UI",
                            "para una interfaz que coloca widgets alrededor de las barras, como Norden UI",
                            "per un'interfaccia che dispone widget intorno alle barre, come Norden UI",
                            "dla interfejsu, który umieszcza widżety wokół pasków, jak Norden UI",
                            "pro rozhraní, které dává widgety kolem lišt, jako Norden UI"],
    "HPM_El_QuestMarker": ["Floating quest marker", "浮遊クエストマーカー", "떠 있는 퀘스트 표시", "浮动任务标记", "Плавающий маркер задания",
                           "Schwebende Questmarkierung", "Marqueur de quête flottant", "Marcador de misión flotante", "Indicatore di missione fluttuante",
                           "Pływający znacznik zadania", "Plovoucí značka úkolu"],
    "HPM_El_Temperature": ["Temperature meter", "体温メーター", "체온 게이지", "体温条", "Шкала температуры", "Temperaturanzeige", "Jauge de température",
                           "Medidor de temperatura", "Indicatore della temperatura", "Wskaźnik temperatury", "Ukazatel teploty"],
    "HPM_El_Breath": ["Breath meter", "息継ぎメーター", "호흡 게이지", "呼吸条", "Шкала дыхания", "Atemanzeige", "Jauge de souffle",
                      "Medidor de aliento", "Indicatore del respiro", "Wskaźnik oddechu", "Ukazatel dechu"],
    "HPM_El_CastingBar": ["Casting bar", "詠唱バー", "시전 바", "施法条", "Полоса заклинания", "Zauberleiste", "Barre d'incantation",
                          "Barra de lanzamiento", "Barra di lancio", "Pasek rzucania", "Ukazatel sesílání"],
    "HPM_El_InfoGold": ["Gold", "所持金", "골드", "金币", "Золото", "Gold", "Or", "Oro", "Oro", "Złoto", "Zlato"],
    "HPM_El_InfoWeight": ["Carry weight", "所持重量", "소지 무게", "负重", "Переносимый вес", "Traglast", "Poids transporté",
                          "Peso cargado", "Peso trasportato", "Udźwig", "Nosnost"],
    "HPM_Style": ["Style", "スタイル", "스타일", "样式", "Стиль", "Stil", "Style", "Estilo", "Stile", "Styl", "Styl"],
    "HPM_StyleHint": ["Two looks for the same information - pick the one you like. A UI mod can reskin either.",
                      "同じ情報を2通りの見た目で表示します。好きな方を選んでください。どちらもUI MODで変更できます。",
                      "같은 정보를 두 가지 모양으로 보여 줍니다. 원하는 쪽을 고르세요. 둘 다 UI 모드로 바꿀 수 있습니다.",
                      "同一信息的两种外观，选你喜欢的。两种都可由界面模组重新换肤。",
                      "Два вида одних и тех же сведений - выберите любой. Оба можно перерисовать модом интерфейса.",
                      "Zwei Ansichten derselben Angabe - wähle die, die dir gefällt. Ein UI-Mod kann beide umgestalten.",
                      "Deux présentations de la même information - choisissez celle qui vous plaît. Un mod d'interface peut habiller l'une ou l'autre.",
                      "Dos aspectos para la misma información: elige el que prefieras. Un mod de interfaz puede cambiar cualquiera de los dos.",
                      "Due aspetti per la stessa informazione: scegli quello che preferisci. Una mod dell'interfaccia può ridisegnarli entrambi.",
                      "Dwa wyglądy tej samej informacji - wybierz ten, który wolisz. Mod interfejsu może zmienić oba.",
                      "Dva vzhledy téže informace - vyberte ten, který se vám líbí. Mod rozhraní může změnit oba."],
    "HPM_StyleBar": ["Bar - the number in front of a bar", "バー - バーの前に数値", "바 - 막대 앞에 숫자", "条形 - 数字在条前",
                     "Полоса - число перед полосой", "Leiste - die Zahl vor einer Leiste", "Barre - le nombre devant une barre",
                     "Barra - el número delante de una barra", "Barra - il numero davanti a una barra", "Pasek - liczba przed paskiem",
                     "Lišta - číslo před lištou"],
    "HPM_StyleBadge": ["Badge - the number in a badge, a ring round it", "バッジ - バッジの中に数値、周りにリング",
                       "배지 - 배지 안에 숫자, 둘레에 고리", "徽章 - 数字在徽章中，外围一圈环",
                       "Значок - число в значке, вокруг кольцо", "Abzeichen - die Zahl in einem Abzeichen, ein Ring darum",
                       "Insigne - le nombre dans un insigne, un anneau autour", "Insignia - el número en una insignia, con un anillo alrededor",
                       "Distintivo - il numero in un distintivo, con un anello intorno", "Odznaka - liczba w odznace, wokół pierścień",
                       "Odznak - číslo v odznaku, kolem něj prstenec"],
    "HPM_El_InfoTime": ["Game time", "ゲーム内時刻", "게임 시간", "游戏时间", "Игровое время", "Spielzeit", "Heure du jeu", "Hora del juego",
                        "Ora di gioco", "Czas w grze", "Herní čas"],
    "HPM_El_ShoutCooldown": ["Shout cooldown", "シャウトの回復", "외침 재사용 대기", "龙吼冷却", "Перезарядка Крика", "Schrei-Abklingzeit",
                             "Recharge du Cri", "Recarga del grito", "Ricarica dell'Urlo", "Odnowienie Krzyku", "Obnova pokřiku"],
    "HPM_El_BowDraw": ["Bow draw", "弓の引き絞り", "활시위 당기기", "拉弓", "Натяжение лука", "Bogenspannung", "Tension de l'arc",
                       "Tensado del arco", "Tensione dell'arco", "Naciąg łuku", "Natažení luku"],
    "HPM_El_ShoutCharge": ["Shout charge", "シャウトの溜め", "외침 충전", "龙吼蓄力", "Зарядка Крика", "Schrei-Aufladung",
                           "Charge du Cri", "Carga del grito", "Carica dell'Urlo", "Ładowanie Krzyku", "Nabití pokřiku"],
    "HPM_El_InfoResist": ["Resistances", "耐性", "저항력", "抗性", "Сопротивления", "Widerstände", "Résistances", "Resistencias",
                          "Resistenze", "Odporności", "Odolnosti"],
    "HPM_El_InfoEquip": ["Equipped items", "装備中のアイテム", "장착한 아이템", "已装备物品", "Экипировка", "Ausgerüstete Gegenstände",
                         "Objets équipés", "Objetos equipados", "Oggetti equipaggiati", "Wyposażone przedmioty", "Vybavené předměty"],
    "HPM_El_InfoPlayTime": ["Play time", "プレイ時間", "플레이 시간", "游玩时间", "Время в игре", "Gespielte Zeit", "Temps de jeu",
                            "Tiempo de juego", "Tempo di gioco", "Czas gry", "Odehraný čas"],
    "HPM_El_InfoEffects": ["Active effects", "有効な効果", "활성 효과", "生效中的效果", "Активные эффекты", "Aktive Effekte",
                           "Effets actifs", "Efectos activos", "Effetti attivi", "Aktywne efekty", "Aktivní efekty"],
    "HPM_El_SurvHunger": ["Hunger", "空腹", "허기", "饥饿", "Голод", "Hunger", "Faim", "Hambre", "Fame", "Głód", "Hlad"],
    "HPM_El_SurvFatigue": ["Fatigue", "疲労", "피로", "疲劳", "Усталость", "Erschöpfung", "Fatigue", "Fatiga", "Stanchezza",
                           "Zmęczenie", "Únava"],
    "HPM_El_SurvCold": ["Cold", "寒さ", "추위", "寒冷", "Холод", "Kälte", "Froid", "Frío", "Freddo", "Zimno", "Zima"],
    "HPM_El_InfoLevel": ["Level", "レベル", "레벨", "等级", "Уровень", "Stufe", "Niveau", "Nivel", "Livello", "Poziom", "Úroveň"],
    "HPM_El_Detection": ["Detection meter", "検知メーター", "탐지 게이지", "侦测条", "Шкала обнаружения", "Entdeckungsanzeige", "Jauge de détection",
                         "Medidor de detección", "Indicatore di rilevamento", "Wskaźnik wykrycia", "Ukazatel odhalení"],
    "HPM_El_TrueHUDHealth": ["TrueHUD health bar", "TrueHUD 体力バー", "TrueHUD 체력 바", "TrueHUD 生命条", "Полоса здоровья TrueHUD", "TrueHUD-Gesundheitsleiste",
                             "Barre de santé TrueHUD", "Barra de salud de TrueHUD", "Barra della salute TrueHUD", "Pasek zdrowia TrueHUD", "Lišta zdraví TrueHUD"],
    "HPM_El_TrueHUDMagicka": ["TrueHUD magicka bar", "TrueHUD マジカバー", "TrueHUD 매지카 바", "TrueHUD 法力条", "Полоса магии TrueHUD", "TrueHUD-Magickaleiste",
                              "Barre de magie TrueHUD", "Barra de magia de TrueHUD", "Barra della magicka TrueHUD", "Pasek magii TrueHUD", "Lišta magie TrueHUD"],
    "HPM_El_TrueHUDStamina": ["TrueHUD stamina bar", "TrueHUD スタミナバー", "TrueHUD 지구력 바", "TrueHUD 耐力条", "Полоса запаса сил TrueHUD", "TrueHUD-Ausdauerleiste",
                              "Barre de vigueur TrueHUD", "Barra de aguante de TrueHUD", "Barra del vigore TrueHUD", "Pasek wytrzymałości TrueHUD", "Lišta výdrže TrueHUD"],
    "HPM_El_TrueHUDOther": ["TrueHUD special bars", "TrueHUD 特殊バー", "TrueHUD 특수 바", "TrueHUD 特殊条", "Особые полосы TrueHUD", "TrueHUD-Sonderleisten",
                            "Barres spéciales TrueHUD", "Barras especiales de TrueHUD", "Barre speciali TrueHUD", "Paski specjalne TrueHUD", "Zvláštní lišty TrueHUD"],
    "HPM_El_WidgetGold": ["Gold widget", "所持金ウィジェット", "골드 위젯", "金币小部件", "Виджет золота", "Gold-Widget", "Widget d'or", "Widget de oro",
                          "Widget dell'oro", "Widżet złota", "Widget zlata"],
    "HPM_El_WidgetWeight": ["Carry weight widget", "所持重量ウィジェット", "소지 무게 위젯", "负重小部件", "Виджет веса", "Traglast-Widget", "Widget de charge",
                            "Widget de peso", "Widget del peso", "Widżet udźwigu", "Widget nosnosti"],
    "HPM_El_WidgetLevel": ["Level widget", "レベルウィジェット", "레벨 위젯", "等级小部件", "Виджет уровня", "Stufen-Widget", "Widget de niveau", "Widget de nivel",
                           "Widget del livello", "Widżet poziomu", "Widget úrovně"],
    "HPM_El_WidgetResist": ["Resistances widget", "耐性ウィジェット", "저항 위젯", "抗性小部件", "Виджет сопротивлений", "Resistenzen-Widget", "Widget des résistances",
                            "Widget de resistencias", "Widget delle resistenze", "Widżet odporności", "Widget odolností"],
    "HPM_El_WidgetEquip": ["Equipment widget", "装備ウィジェット", "장비 위젯", "装备小部件", "Виджет снаряжения", "Ausrüstungs-Widget", "Widget d'équipement",
                           "Widget de equipo", "Widget dell'equipaggiamento", "Widżet ekwipunku", "Widget výbavy"],
    "HPM_El_WidgetShout": ["Shout widget", "シャウトウィジェット", "용언 위젯", "龙吼小部件", "Виджет Крика", "Schrei-Widget", "Widget de cri", "Widget de grito",
                           "Widget dell'urlo", "Widżet okrzyku", "Widget pokřiku"],
    "HPM_El_WidgetGameTime": ["Game time widget", "ゲーム内時刻ウィジェット", "게임 시간 위젯", "游戏时间小部件", "Виджет игрового времени", "Spielzeit-Widget",
                              "Widget de l'heure du jeu", "Widget de hora del juego", "Widget dell'ora di gioco", "Widżet czasu w grze", "Widget herního času"],
    "HPM_El_WidgetPlayTime": ["Play time widget", "プレイ時間ウィジェット", "플레이 시간 위젯", "游玩时长小部件", "Виджет времени игры", "Spieldauer-Widget",
                              "Widget du temps de jeu", "Widget de tiempo jugado", "Widget del tempo di gioco", "Widżet czasu gry", "Widget odehraného času"],
    "HPM_El_WidgetOxygen": ["Oxygen meter", "酸素メーター", "산소 게이지", "氧气条", "Шкала кислорода", "Sauerstoffanzeige", "Jauge d'oxygène", "Medidor de oxígeno",
                            "Indicatore dell'ossigeno", "Wskaźnik tlenu", "Ukazatel kyslíku"],
    "HPM_El_WidgetCasting": ["Casting bar", "詠唱バー", "시전 바", "施法条", "Полоса заклинания", "Zauberleiste", "Barre d'incantation", "Barra de lanzamiento",
                             "Barra di lancio", "Pasek rzucania", "Ukazatel sesílání"],
}
LANGS = ["english", "japanese", "korean", "chinese", "russian", "german", "french", "spanish", "italian", "polish", "czech"]


def or_translations():
    """The Oblivion Remastered version's NEW table (same key names, same language order) - reused where the English matches."""
    src = open(OR_GENDIST, encoding="utf-8").read()
    start = src.index("NEW = {") + len("NEW = ")
    i, depth = start, 0
    while True:
        depth += {"{": 1, "}": -1}.get(src[i], 0)
        i += 1
        if depth == 0:
            break
    return ast.literal_eval(src[start:i])


OR = or_translations()
for k, _ in elements:
    assert f"HPM_El_{k}" in T, f"no translation of the element name HPM_El_{k}"
# every TR("key", "English") in the source: the English must be the table's, and every key must have eleven languages
used = {}
for fn in os.listdir(R + r"\source"):
    if fn.endswith(".cpp"):
        for k, en in re.findall(r'TR\("(HPM_[A-Za-z]+)", "((?:[^"\\]|\\.)*)"\)', open(os.path.join(R, "source", fn), encoding="utf-8").read()):
            used[k] = en
problems = []
for k, en in sorted(used.items()):
    if k in T and T[k][0] == en:
        continue
    if k in OR and OR[k][0] == en:
        T[k] = OR[k]
        continue
    have = T.get(k, OR.get(k, [None]))[0]
    problems.append(f"{k}: source says {en!r}, table says {have!r}")
assert not problems, "translations missing or out of date:\n  " + "\n  ".join(problems)
for i, lang in enumerate(LANGS):
    lines = [f"${k}\t{v[i]}" for k, v in T.items()]
    data = "\ufeff" + "\r\n".join(lines) + "\r\n"
    open(R + rf"\dist\Interface\Translations\HUDPositionManager_{lang}.txt", "wb").write(data.encode("utf-16-le"))
print(len(T), "keys x", len(LANGS), "languages;", len(elements), "elements;", len(used), "keys used in the source")
