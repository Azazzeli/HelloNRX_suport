// Совместимость с Model Studio / ObjectARX
#define _ARXVER_202X
#define _ACRX_VER
// #define NCAD           // раскомментируйте при сборке под nanoCAD
#define _MSC_VER 1920    // под вашу версию VS

#include "stdafx.h"
#include <windows.h>
#include <tchar.h>

// ObjectARX
#include "acdb.h"
#include "dbmain.h"
#include "dbapserv.h"
#include "geassign.h"
#include "rxregsvc.h"
#include "acgi.h"

// ViperCS / Model Studio SDK
#include "vCSCreatePipe.h"
#include "vCSUtils.h"
#include "..\ViperCSObj\vCSDragManager.h"
#include "..\ViperCSObj\vCSDragManagerSmart.h"
#include "..\ViperCSObj\vCSSupport.h"          // eSupportType
#include "..\ViperCSObj\vCS_DM_Axis.h"
#include "..\ViperCSObj\vCS_DM_Seg.h"
#include "..\ViperCSObj\vCSSettingsTracingObj.h"

void helloNrxCmd()
{
    acutPrintf(L"\nHello, NRX! Testing ViperCS SDK...\n");

    CString msg;
    if (CVCSUtils::CheckModel(msg))
        acutPrintf(L"✅ Модель корректна.\n");
    else
        acutPrintf(L"❌ Ошибки: %ls\n", msg.GetBuffer());

    // 1) Создать трубу по двум точкам
    AcGePoint3dArray path;
    path.append(AcGePoint3d(0.0, 0.0, 0.0));
    path.append(AcGePoint3d(1000.0, 0.0, 0.0)); // 1 м вправо

    AcDbObjectId newAxisId = AcDbObjectId::kNull;
    bool bSuccess = vCSCreatePipeOnPoints::Create(path, newAxisId);

    if (!(bSuccess && !newAxisId.isNull()))
    {
        acutPrintf(L"\n❌ Не удалось создать трубу.");
        return;
    }
    acutPrintf(L"Труба создана.\n");

    // 2) Автовыбор параметров опоры с созданной трубы
    vCSDragManagerSmart dms;           // RAII DM + транзакция
    vCSDragManager* dm = dms.operator->();

    vCS_DM_Axis* pAxis = dm->FindAxis(newAxisId);
    if (!pAxis || pAxis->GetSegCount() == 0)
    {
        acutPrintf(L"\n⚠ Не найден сегмент оси.");
        return;
    }

    vCS_DM_Seg* pSeg = pAxis->GetSeg(0);
    if (!pSeg)
    {
        acutPrintf(L"\n⚠ Сегмент недоступен.");
        return;
    }

    // OID сегмента
    AcDbObjectId segId = pSeg->OID();

    // Профиль в начале сегмента
    vCSProfilePtr profPtr;
    AcGeVector3d vOff;
    if (!pSeg->ArrDistProfileGetByDist(0.0, profPtr, vOff) || profPtr.IsNull())
    {
        acutPrintf(L"\n⚠ Не удалось получить профиль сегмента.");
        return;
    }
    const vCSProfileBase* pSubSegProfile = profPtr; // implicit cast

    // Точка вставки — середина сегмента
    AcGePoint3d ptStart = pSeg->GetStartPoint();
    AcGePoint3d ptEnd = pSeg->GetEndPoint();
    AcGePoint3d ptInsert(
        (ptStart.x + ptEnd.x) * 0.5,
        (ptStart.y + ptEnd.y) * 0.5,
        (ptStart.z + ptEnd.z) * 0.5
    );

    dm->m_SupportCreate.ClearDMTypes();
    dm->m_SupportCreate.SetPickedSegID(segId);

    bool checkMinidir = true;
    bool bDialogDraw = false;
    unsigned int supportType = st_Support; // st_Grill / st_GrillTerm и т.п.
    bool bGrillIsStart = true;
    bool bSupAxisTracking = false;

    Acad::ErrorStatus es = dm->m_SupportCreate.ReCalculateModelMainSupportCreate(
        segId,
        pSubSegProfile,
        ptInsert,
        checkMinidir,
        bDialogDraw,
        AcDbObjectId::kNull,   // idTemplate при необходимости
        nullptr,               // CElement* параметры, если нужно
        supportType,
        bGrillIsStart,
        bSupAxisTracking
    );

    if (es == Acad::eOk)
    {
        dms.commit();
        acutPrintf(L"\n✅ Опора создана автоматически по параметрам трубы.");
    }
    else
    {
        acutPrintf(L"\n❌ Ошибка создания опоры: %d", es);
    }
}

extern "C" __declspec(dllexport) AcRx::AppRetCode
ncrxEntryPoint(AcRx::AppMsgCode msg, void* appId)
{
    switch (msg)
    {
    case AcRx::kInitAppMsg:
        acrxDynamicLinker->unlockApplication(appId);
        acrxDynamicLinker->registerAppMDIAware(appId);
        acedRegCmds->addCommand(L"HELLONRX_GROUP", L"_HELLONRX", L"HELLONRX", ACRX_CMD_TRANSPARENT, helloNrxCmd);
        break;
    case AcRx::kUnloadAppMsg:
        acedRegCmds->removeGroup(L"HELLONRX_GROUP");
        break;
    }
    return AcRx::kRetOK;
}