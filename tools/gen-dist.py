"""The shipped INI and the eleven translation files for HUD Position Manager."""
import os, re

R = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
os.makedirs(R + r"\dist\SKSE\Plugins", exist_ok=True)
os.makedirs(R + r"\dist\Interface\Translations", exist_ok=True)

elements = re.findall(r'\{ "([A-Za-z]+)", "([^"]+)", \{', open(R + r"\source\Elements.cpp", encoding="utf-8").read())
ini = ["; HUD Position Manager - its settings page in the Apocrypha Menu Framework edits this file for you.",
       "; Offsets are in HUD units (the HUD's 1280x720 stage) and are added to where the HUD itself puts the element.",
       "; fScale is times the size the HUD gives it, about the element's centre. bHide hides the element.",
       "", "[General]", "; 1 = apply the layout below, 0 = every element back where the HUD puts it", "bEnabled=1",
       "; 1 = outline the element whose tab is open on the settings page", "bHighlight=1",
       "; Log level: 0 trace, 1 debug, 2 info, 3 warn, 4 error. Raise to 0 for a bug report.", "uLogLevel=2"]
for key, _name in elements:
    ini += ["", f"[{key}]", "fOffsetX=0", "fOffsetY=0", "fScale=1", "bHide=0"]
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
    "HPM_Highlight": ["Outline the element being edited", "編集中の要素を枠で囲む", "편집 중인 요소에 테두리 표시", "为正在编辑的元素加框",
                      "Обводить редактируемый элемент", "Das bearbeitete Element umrahmen", "Encadrer l'élément en cours de modification",
                      "Resaltar el elemento que se edita", "Evidenzia l'elemento in modifica", "Obrysuj edytowany element", "Zvýraznit upravovaný prvek"],
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
}
LANGS = ["english", "japanese", "korean", "chinese", "russian", "german", "french", "spanish", "italian", "polish", "czech"]
for k, _ in elements:
    assert f"HPM_El_{k}" in T, k
used = set(re.findall(r'strings::TR\("(HPM_[A-Za-z]+)"', open(R + r"\source\UI.cpp", encoding="utf-8").read()))
missing = used - set(T)
assert not missing, missing
for i, lang in enumerate(LANGS):
    lines = [f"${k}\t{v[i]}" for k, v in T.items()]
    data = "\ufeff" + "\r\n".join(lines) + "\r\n"
    open(R + rf"\dist\Interface\Translations\HUDPositionManager_{lang}.txt", "wb").write(data.encode("utf-16-le"))
print(len(T), "keys x", len(LANGS), "languages;", len(elements), "elements")
