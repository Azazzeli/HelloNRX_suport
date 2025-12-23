// Совместимость с Model Studio / nanoCAD
#define _ARXVER_202X
#define _ACRX_VER
#define NCAD            // nanoCAD
#define _MSC_VER 1920   // под вашу VS

#include "stdafx.h"
#include <windows.h>
#include <tchar.h>

// ObjectARX / nanoCAD SDK
#include "acdb.h"
#include "dbmain.h"
#include "dbapserv.h"
#include "geassign.h"
#include "rxregsvc.h"
#include "acgi.h"
#include "aced.h"

// ViperCS / Model Studio SDK
#include "vCSCreatePipe.h"
#include "vCSUtils.h"
#include "..\ViperCSObj\vCSDragManager.h"
#include "..\ViperCSObj\vCSDragManagerSmart.h"
#include "..\ViperCSObj\vCSSupport.h"          // eSupportType
#include "..\ViperCSObj\vCS_DM_Axis.h"
#include "..\ViperCSObj\vCS_DM_Seg.h"
#include "..\ViperCSObj\vCSSettingsTracingObj.h"
#include "..\ViperCSObj\vCSTools.h"             // GetSegments

void helloNrxCmd()
{
    acutPrintf(L"\nВыберите трубу (ось/сегмент/подсегмент): ");
    ads_name en; AcGePoint3d pickPt;
    if (acedEntSel(L"\nТруба: ", en, asDblArray(pickPt)) != RTNORM) {
        acutPrintf(L"\nОтмена.");
        return;
    }
    AcDbObjectId idEnt; acdbGetObjectId(idEnt, en);

    vCSDragManagerSmart dms;           // RAII DM + транзакция
    vCSDragManager* dm = dms.operator->();

    // --- определяем ось и собираем сегменты по базе ---
    AcDbObjectId idAxis = dm->GetIdAxisBySegmentId(idEnt);
    if (idAxis.isNull()) idAxis = dm->GetIdAxisByEntityId(idEnt);
    if (idAxis.isNull()) idAxis = CVCSUtils::getAxis(idEnt);

    AcDbObjectIdArray segIds;
    if (!idAxis.isNull())
        vCSTools::GetSegments(idAxis, segIds); // сегменты оси из БД

    // прямой поиск сегмента по OID
    vCS_DM_Seg* pSegBest = dm->GetSeg(idEnt);
    AcGePoint3d ptClosestBest = pickPt;
    double dist2Best = 1e300;

    // перебор сегментов из БД: проверяем подсегменты и ищем ближайший
    for (int i = 0; i < segIds.length(); ++i) {
        vCS_DM_Seg* s = dm->GetSeg(segIds[i]);
        if (!s) continue;

        AcDbObjectIdArray ssids;
        s->GetSubSegments(ssids);
        s->GetSubSegmentsX(ssids);
        for (int k = 0; k < ssids.length(); ++k) {
            if (ssids[k] == idEnt) pSegBest = s;
        }

        AcGePoint3d ptClosest;
        s->get_closest_point(pickPt, ptClosest);
        AcGeVector3d d = ptClosest - pickPt;
        double d2 = d.lengthSqrd();
        if (d2 < dist2Best) {
            dist2Best = d2;
            ptClosestBest = ptClosest;
            if (!pSegBest) pSegBest = s;
        }
    }

    // запасной вариант: через FindAxis, если segIds пуст или не сработали
    if (!pSegBest && !idAxis.isNull()) {
        vCS_DM_Axis* pAxis = dm->FindAxis(idAxis);
        if (pAxis) {
            int segCount = pAxis->GetSegCount();
            for (int i = 0; i < segCount; ++i) {
                vCS_DM_Seg* s = pAxis->GetSeg(i);
                if (!s) continue;
                AcGePoint3d ptClosest;
                s->get_closest_point(pickPt, ptClosest);
                AcGeVector3d d = ptClosest - pickPt;
                double d2 = d.lengthSqrd();
                if (d2 < dist2Best) {
                    dist2Best = d2;
                    ptClosestBest = ptClosest;
                    pSegBest = s;
                }
            }
        }
    }

    if (!pSegBest) {
        acutPrintf(L"\n⚠ Не удалось выбрать сегмент.");
        return;
    }

    AcDbObjectId segId = pSegBest->OID();

    double distFromPrev = 0.0;
    pSegBest->GetDistFromPrev(ptClosestBest, distFromPrev, true);

    vCSProfilePtr profPtr;
    AcGeVector3d vOff;
    if (!pSegBest->ArrDistProfileGetByDist(distFromPrev, profPtr, vOff) || profPtr.IsNull()) {
        acutPrintf(L"\n⚠ Не удалось получить профиль сегмента.");
        return;
    }
    const vCSProfileBase* pSubSegProfile = profPtr; // implicit cast

    AcGePoint3d ptInsert = ptClosestBest;

    dm->m_SupportCreate.ClearDMTypes();
    dm->m_SupportCreate.SetPickedSegID(segId);

    bool checkMinidir = true;
    bool bDialogDraw = false;
    unsigned int supportType = st_Support; // st_Grill / st_GrillTerm при необходимости
    bool bGrillIsStart = true;
    bool bSupAxisTracking = false;

    Acad::ErrorStatus es = dm->m_SupportCreate.ReCalculateModelMainSupportCreate(
        segId,
        pSubSegProfile,
        ptInsert,
        checkMinidir,
        bDialogDraw,
        AcDbObjectId::kNull,   // idTemplate при необходимости
        nullptr,               // CElement* при необходимости
        supportType,
        bGrillIsStart,
        bSupAxisTracking
    );

    if (es == Acad::eOk) {
        dms.commit();
        acutPrintf(L"\n✅ Опора создана на выбранной трубе.");
    }
    else {
        acutPrintf(L"\n❌ Ошибка создания опоры: %d", es);
    }
}

// Только для nanoCAD: экспортируем ncrxEntryPoint
extern "C" __declspec(dllexport) AcRx::AppRetCode
ncrxEntryPoint(AcRx::AppMsgCode msg, void* appId)
{
    switch (msg)
    {
    case AcRx::kInitAppMsg:
        acrxDynamicLinker->unlockApplication(appId);
        acrxDynamicLinker->registerAppMDIAware(appId);
        acedRegCmds->addCommand(L"HELLONRX_GROUP", L"_HELLONRX", L"HELLONRX", ACRX_CMD_MODAL, helloNrxCmd);
        break;
    case AcRx::kUnloadAppMsg:
        acedRegCmds->removeGroup(L"HELLONRX_GROUP");
        break;
    }
    return AcRx::kRetOK;
}