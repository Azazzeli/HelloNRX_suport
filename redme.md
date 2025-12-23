

## Базовые принципы
- Работать с DM через `vCSDragManagerSmart` (RAII: создаёт/закрывает транзакцию, `commit()` по успеху).
- Единый DM: `vCSDragManager* dm = dms.operator->();`
- Основные сущности DM:
  - Оси: `vCS_DM_Axis` (найти `dm->FindAxis(idAxis)`).
  - Сегменты: `vCS_DM_Seg` (`dm->GetSeg(id)` или по оси `axis->GetSeg(i)`).
  - Подсегменты: хранятся в `vCS_DM_Seg` (см. `GetSubSegments`, `GetSubSegmentsX`).
  - Профиль по длине сегмента: `ArrDistProfileGetByDist(dist, vCSProfilePtr&, AcGeVector3d&)`.
- Коммит: после успешного пересчёта/создания — `dms.commit();`.

## Поток выбора трубы/сегмента (шаблон)
1) Пользователь выбирает объект (`acedEntSel` → `idEnt`, `pickPt`).
2) Находим ось:
   - `dm->GetIdAxisBySegmentId(idEnt)` → `dm->GetIdAxisByEntityId(idEnt)` → `CVCSUtils::getAxis(idEnt)`.
3) Сегменты оси: `vCSTools::GetSegments(idAxis, segIds)`.
4) Сегмент:
   - Прямо: `dm->GetSeg(idEnt)`.
   - По подсегментам: обойти `segIds`, проверять `GetSubSegments`, `GetSubSegmentsX`.
   - Если не найден — ближайший к `pickPt` через `get_closest_point`.
5) Расстояние от начала сегмента: `GetDistFromPrev(ptClosest, dist, true)`.
6) Профиль: `ArrDistProfileGetByDist(dist, profPtr, vOff)`; проверять `!profPtr.IsNull()`.
7) Точка вставки: проекция `pickPt` на сегмент (`get_closest_point`).

## Создание опоры (Support)
- Класс: `dm->m_SupportCreate`.
- Перед вызовом: `ClearDMTypes()`, `SetPickedSegID(segId)`.
- Основной метод:  
  `ReCalculateModelMainSupportCreate(segId, pSubSegProfile, ptInsert, checkMinidir, bDialogDraw, idTemplate, pRoot, supportType, bGrillIsStart, bSupAxisTracking)`
- Если нет шаблона — `idTemplate = AcDbObjectId::kNull`.
- После `Acad::eOk` → `commit()`.

## Создание арматуры (Inline) — без шаблона
- Класс: `dm->m_InLineCreate`.
- Перед вызовом: `ClearDMTypes()`, `SetPickedSegID(segId)`.
- Использовать перегрузку с `nType`:  
  `ReCalculateModelMainInLineCreate(segId, pSubSegProfile, ptInsert, nType, pRoot, bBreak, pL1_L2);`
- `nType` из `vCSILBase::ETypeInLine` (пример: `til_inline`, `til_reducer`, `til_tee`, `til_connection`, `til_offset`).
- Если нужен шаблон (готовый InLine на чертеже) — другая перегрузка с `idTemplate` (указатель на объект-шаблон).

## Ключевые методы DragManager, которые пригодятся
- Получение сущностей по OID:  
  `GetSeg`, `GetNode`, `GetInLine`, `GetSupport`, `GetWeld`.
- Поиск оси: `FindAxis(idAxis)`.
- Поиск OID оси по сущности: `GetIdAxisBySegmentId`, `GetIdAxisByEntityId`.
- Работа с подсегментами: у сегмента `GetSubSegments`, `GetSubSegmentsX`; в DM есть массивы выбранных подсегментов (см. `m_ArrPtrSubSeg*`).
- Start/End drag, Draw: `StartDragging`, `EndDragging`, `DrawAcGsModel` (если нужно явно).
- Настройки трассировки: `getSettingsTracing`, `setSettingsTracing`.
- Кэш библиотечных объектов: `GetCachedComponent*` (если подхватываете компоненты из библиотеки).

## Ошибки/коды
- `Acad::eOk` — успех.
- `eInvalidInput` (код 5) — часто из-за неправильного вызова (например, для InLine без шаблона нужно вызывать перегрузку с `nType`, а не с `idTemplate = kNull`).

## Точки входа (nanoCAD 24)
- Используйте только `ncrxEntryPoint`, без `acrxEntryPoint`.
- Регистрация команд:
  ```cpp
  acedRegCmds->addCommand(L"HELLONRX_GROUP",
                          L"_HELLONRX", L"HELLONRX",
                          ACRX_CMD_MODAL, helloNrxCmd);

  acedRegCmds->addCommand(L"HELLONRX_GROUP",
                          L"_VALVE", L"VALVE",
                          ACRX_CMD_MODAL, helloArmCmd);
  ```
- В исходниках определяйте `NCAD`.

## Расширение под другие типы
- Вставка сварки: аналогично InLine, но с `m_WeldCreate` или через тип `til_weld`.
- Перемещения/drag: использовать соответствующие классы внутри DM (`m_InLineMove`, `m_SupportMove`, `m_WeldMove`) — паттерн тот же: задать picked IDs, вызвать `ReCalc_*`, при успехе `commit()`.
- Опции подсегментов: если нужно жёстко привязываться к подсегменту — храните выбранный `subSegId` и учитывайте его при вычислении `distFromPrev` (проекция точки на нужный подсегмент).

## Минимальный каркас для любой операции
```cpp
vCSDragManagerSmart dms;
vCSDragManager* dm = dms.operator->();
// ... поиск оси/сегмента/профиля/точки ...
// ... вызов нужного ReCalculateModelMain* ...
if (es == Acad::eOk) dms.commit();
```

Следуя этим шагам, вы сможете вызывать остальные методы DragManager по аналогии: найти ось/сегмент, получить профиль в точке, вызвать нужный `ReCalculateModelMain*` или `ReCalc_*`, и зафиксировать изменения через `commit()`.