/*
 * @coding: utf-8
 * @Author: vector-wlc
 * @Date: 2020-02-06 10:22:46
 * @Description:
 */

#include "avz_asm.h"
#include "avz_card.h"
#include "avz_click.h"
#include "avz_memory.h"
#include "avz_smart.h"

////////////////////////////////////////
//  ItemCollector
////////////////////////////////////////

AItemCollector::AItemCollector()
{
    _types.fill(true);
}

void AItemCollector::_EnterFight()
{
    AItemCollector::Start();
}

void AItemCollector::SetInterval(int timeInterval)
{
    if (timeInterval < 1) {
        auto&& pattern = __aig.loggerPtr->GetPattern();
        __aig.loggerPtr->Error("自动收集类时间间隔范围为:[1, 正无穷], 你现在设定的参数为 " + pattern, timeInterval);
        return;
    }
    this->_timeInterval = timeInterval;
}

void AItemCollector::SetTypeList(const std::vector<int>& types)
{
    _types.fill(false);
    for (auto type : types) {
        if (type < 1 && type >= _TYPE_SIZE) {
            AGetInternalLogger()->Warning("AItemCollector::SetTypeList : 您设置的收集物类型: {} 不存在", type);
            continue;
        }
        _types[type] = true;
    }
}

void AItemCollector::Start()
{
    ATickRunnerWithNoStart::_Start([this] { _Run(); }, ONLY_FIGHT);
}

void AItemCollector::_Run()
{
    if (AGetMainObject()->GameClock() % _timeInterval != 0 || //
        AGetMainObject()->MouseAttribution()->Type() != 0) {
        return;
    }

    auto item = AGetMainObject()->ItemArray();
    int total = AGetMainObject()->ItemTotal();
    int collectIdx = -1;
    for (int index = 0; index < total; ++index, ++item) {
        int type = item->Type();
        if (item->IsCollected() || item->IsDisappeared() || !_types[item->Type()]) {
            continue;
        }
        collectIdx = index;
        if (ARangeIn(type, {4, 5, 6})) { // 优先采集阳光
            break;
        }
    }
    if (collectIdx == -1) { // 没有要收集的物品
        return;
    }

    item = AGetMainObject()->ItemArray() + collectIdx;
    float itemX = item->Abscissa();
    float itemY = item->Ordinate();
    if (itemX >= 0.0 && itemY >= 70) {
        AAsm::ReleaseMouse();
        int x = static_cast<int>(itemX + 30);
        int y = static_cast<int>(itemY + 30);
        ALeftClick(x, y);
        AAsm::ReleaseMouse();
    }
}

////////////////////////////////////////
//  IceFiller
////////////////////////////////////////

void AIceFiller::SetIceSeedList(const std::vector<int>& lst)
{
    // 未运行即进行检查是否为冰卡
    for (const auto& seedType : lst) {
        if (seedType != AICE_SHROOM && seedType != AM_ICE_SHROOM) {
            auto&& pattern = __aig.loggerPtr->GetPattern();
            __aig.loggerPtr->Error(
                "resetIceSeedList : 您填写的参数为 " + pattern + //
                    " , 然而此函数只接受植物类型为寒冰菇或模仿寒冰菇的参数",
                seedType);
            return;
        }
    }

    _iceSeedIdxVec.clear();
    int iceIdx = 0;
    for (const auto& seedType : lst) {
        iceIdx = AGetSeedIndex(AICE_SHROOM, seedType / AM_PEASHOOTER);
        if (iceIdx == -1) {
            __aig.loggerPtr->Error("resetIceSeedList : 您没有选择对应的冰卡");
            continue;
        }
        _iceSeedIdxVec.push_back(iceIdx);
    }
}

void AIceFiller::EraseFromList(const std::vector<AGrid>& lst)
{
    _fillIceGridVec = __AErase(_fillIceGridVec, lst);
}

void AIceFiller::EraseFromList(int row, int col)
{
    EraseFromList({{row, col}});
}

void AIceFiller::MoveToListTop(const std::vector<AGrid>& lst)
{
    _fillIceGridVec = __AMoveToTop(_fillIceGridVec, lst);
}

void AIceFiller::MoveToListTop(int row, int col)
{
    MoveToListTop({{row, col}});
}

void AIceFiller::MoveToListBottom(const std::vector<AGrid>& lst)
{
    _fillIceGridVec = __AMoveToBottom(_fillIceGridVec, lst);
}

void AIceFiller::MoveToListBottom(int row, int col)
{
    MoveToListBottom({{row, col}});
}

void AIceFiller::Start(const std::vector<AGrid>& lst)
{
    _iceSeedIdxVec.clear();
    int iceSeedIdx = AGetSeedIndex(AICE_SHROOM);
    if (iceSeedIdx != -1) {
        _iceSeedIdxVec.push_back(iceSeedIdx);
    }
    iceSeedIdx = AGetSeedIndex(AICE_SHROOM, true);
    if (iceSeedIdx != -1) {
        _iceSeedIdxVec.push_back(iceSeedIdx);
    }
    _coffeeSeedIdx = AGetSeedIndex(ACOFFEE_BEAN);
    _fillIceGridVec = lst;
    ATickRunnerWithNoStart::_Start([this]() { _Run(); }, ONLY_FIGHT);
}

void AIceFiller::_Run()
{
    ASeed* seed = nullptr;
    auto iceSeedIdxIter = _iceSeedIdxVec.begin();
    for (; iceSeedIdxIter != _iceSeedIdxVec.end(); ++iceSeedIdxIter) {
        auto tmpSeed = AGetMainObject()->SeedArray() + *iceSeedIdxIter;
        if (AIsSeedUsable(tmpSeed)) {
            seed = tmpSeed;
            break;
        }
    }
    if (seed == nullptr) {
        return;
    }

    for (auto fillIceGridIter = _fillIceGridVec.begin();
         fillIceGridIter != _fillIceGridVec.end();
         ++fillIceGridIter) {
        if (AAsm::GetPlantRejectType(AICE_SHROOM, fillIceGridIter->row - 1, fillIceGridIter->col - 1) != AAsm::NIL) {
            continue;
        }
        ACard(*iceSeedIdxIter + 1, fillIceGridIter->row,
            fillIceGridIter->col);
        ++fillIceGridIter;
        break;
    }
}

void AIceFiller::Coffee()
{
    if (_coffeeSeedIdx == -1) {
        __aig.loggerPtr->Error("你没有选择咖啡豆卡片!");
        return;
    }
    if (_fillIceGridVec.empty()) {
        __aig.loggerPtr->Error("你还未为自动存冰对象初始化存冰列表");
        return;
    }
    auto icePlantIdxVec = AGetPlantIndices(_fillIceGridVec, AICE_SHROOM);
    for (int i = _fillIceGridVec.size() - 1; i >= 0; --i) {
        if (icePlantIdxVec[i] < 0) {
            continue;
        }
        AAsm::ReleaseMouse();
        ACard(_coffeeSeedIdx + 1, _fillIceGridVec[i].row, _fillIceGridVec[i].col);
        AAsm::ReleaseMouse();
        return;
    }
    __aig.loggerPtr->Error("Coffee : 未找到可用的存冰");
}

/////////////////////////////////////////////////
//    PlantFixer
/////////////////////////////////////////////////

void APlantFixer::EraseFromList(const std::vector<AGrid>& lst)
{
    _gridLst = __AErase(_gridLst, lst);
}

void APlantFixer::EraseFromList(int row, int col)
{
    EraseFromList({{row, col}});
}

void APlantFixer::MoveToListTop(const std::vector<AGrid>& lst)
{
    _gridLst = __AMoveToTop(_gridLst, lst);
}

void APlantFixer::MoveToListTop(int row, int col)
{
    MoveToListTop({{row, col}});
}

void APlantFixer::MoveToListBottom(const std::vector<AGrid>& lst)
{
    _gridLst = __AMoveToBottom(_gridLst, lst);
}

void APlantFixer::MoveToListBottom(int row, int col)
{
    MoveToListBottom({{row, col}});
}

void APlantFixer::AutoSetList()
{
    _gridLst.clear();
    auto plant = AGetMainObject()->PlantArray();
    int plantCntMax = AGetMainObject()->PlantCountMax();
    AGrid grid;
    for (int index = 0; index < plantCntMax; ++index, ++plant) {
        if (!plant->IsCrushed() && !plant->IsDisappeared() && plant->Type() == _plantType) {
            grid.col = plant->Col() + 1;
            grid.row = plant->Row() + 1;
            _gridLst.push_back(grid);
        }
    }
}

void APlantFixer::_UseSeed(int seed_index, int row, float col,
    bool isNeedShovel)
{
    if (isNeedShovel) {
        // {_plantType, _plantType} 会转成 std::vector
        // 但是 {_plantType} 会转成 int
        ARemovePlant(row, col, {_plantType, _plantType});
    }
    ACard(seed_index + 1, row, col);
    if (_isUseCoffee) {
        ACard(_coffeeSeedIdx + 1, row, col);
    }
}

void APlantFixer::_GetSeedList()
{
    _seedIdxVec.clear();
    int seedIndex;
    seedIndex = AGetSeedIndex(_plantType);
    if (-1 != seedIndex) {
        _seedIdxVec.push_back(seedIndex);
    }
    seedIndex = AGetSeedIndex(_plantType, true);

    if (-1 != seedIndex) {
        _seedIdxVec.push_back(seedIndex);
    }
    if (_seedIdxVec.size() == 0) {
        __aig.loggerPtr->Error("您没有选择修补该植物的卡片！");
    }
    _coffeeSeedIdx = AGetSeedIndex(ACOFFEE_BEAN);
}

void APlantFixer::Start(int plantType, const std::vector<AGrid>& lst,
    int fixHp)
{
    if (plantType == ACOFFEE_BEAN) {
        __aig.loggerPtr->Error("PlantFixer 不支持修补咖啡豆");
        return;
    }

    if (plantType >= AGATLING_PEA) {
        __aig.loggerPtr->Error("修补植物类仅支持绿卡");
        return;
    }

    _plantType = plantType;
    _fixHp = fixHp;
    _GetSeedList();
    // 如果没有给出列表信息
    if (lst.size() == 0) {
        AutoSetList();
    } else {
        _gridLst = lst;
    }
    ATickRunnerWithNoStart::_Start([this]() { _Run(); }, ONLY_FIGHT);
}

void APlantFixer::_Run()
{
    auto usableSeedIndexIter = _seedIdxVec.begin();
    if (usableSeedIndexIter == _seedIdxVec.end()) {
        return;
    }

    if (_isUseCoffee) {
        if (_coffeeSeedIdx == -1) {
            return;
        }
        auto coffeeSeed = AGetMainObject()->SeedArray() + _coffeeSeedIdx;
        if (!AIsSeedUsable(coffeeSeed)) {
            return;
        }
    }

    do {
        auto seedMemory = AGetMainObject()->SeedArray();
        seedMemory += *usableSeedIndexIter;
        if (AIsSeedUsable(seedMemory)) {
            break;
        }
        ++usableSeedIndexIter;
    } while (usableSeedIndexIter != _seedIdxVec.end());

    // 没找到可用的卡片
    if (usableSeedIndexIter == _seedIdxVec.end()) {
        return;
    }
    auto plantIdxVec = AGetPlantIndices(_gridLst, _plantType);
    AGrid needPlantGrid = {0, 0};
    int minHp = _fixHp;
    auto gridIter = _gridLst.begin();
    auto plantIdxIter = plantIdxVec.begin();
    for (; gridIter != _gridLst.end(); ++gridIter, ++plantIdxIter) {
        // 如果此处存在除植物类植物的植物
        if (*plantIdxIter == -2) {
            continue;
        }

        if (*plantIdxIter == -1) {
            if (AAsm::GetPlantRejectType(_plantType, gridIter->row - 1, gridIter->col - 1) != AAsm::NIL) {
                continue;
            }
            _UseSeed((*usableSeedIndexIter), gridIter->row, gridIter->col,
                false);
            return;
        }
        auto plant = AGetMainObject()->PlantArray();
        plant += *plantIdxIter;
        int plantHp = plant->Hp();
        // 如果当前生命值低于最小生命值，记录下来此植物的信息
        if (plantHp < minHp) {
            minHp = plantHp;
            needPlantGrid.row = gridIter->row;
            needPlantGrid.col = gridIter->col;
        }
    }

    // 如果有需要修补的植物且植物卡片能用则进行种植
    if (needPlantGrid.row) {
        // 种植植物
        _UseSeed((*usableSeedIndexIter), needPlantGrid.row,
            needPlantGrid.col, true);
    }
}
