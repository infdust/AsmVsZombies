/*
 * @Coding: utf-8
 * @Author: vector-wlc
 * @Date: 2022-11-10 15:28:33
 * @Description:
 */

#include "libavz.h"

void __AOpQueueManager::_SetRefreshTime(int wave, int refreshTime)
{
    auto queueIter = container.begin() + wave;
    queueIter->memRefreshTime = refreshTime;
    queueIter->calRefreshTime = refreshTime;

    if (wave != 0) {
        auto wavelength = queueIter->memRefreshTime - (queueIter - 1)->memRefreshTime;
        __aig.loggerPtr->Info("下一波即将刷新，第 " + std::to_string(wave) + " 波的波长为 " + std::to_string(wavelength));
    }

    auto nextQueueIter = queueIter + 1;
    while (nextQueueIter != container.end() && queueIter->wavelength != -1) {
        nextQueueIter->calRefreshTime = queueIter->calRefreshTime + queueIter->wavelength;
        ++queueIter;
        ++nextQueueIter;
    }
}

std::optional<__ATimeIter>
__AOpQueueManager::Push(const ATime& time, __ABoolOperation&& timeOp)
{
    if (time.wave < 1 || time.wave > container.size()) {
        __aig.loggerPtr->Error(
            "您连接设定的 wave 参数为 " + //
            std::to_string(time.wave) + ", 超出有效范围");
        return std::nullopt;
    }

    if (time.wave < startTime.wave || //
        (time.wave == startTime.wave && time.time < startTime.time)) {
        __aig.loggerPtr->Info("当前连接时间 ("
            + std::to_string(time.wave) + ", " + std::to_string(time.time)
            + ") 小于开始时间 (" + std::to_string(time.wave) + ", " + std::to_string(time.time)
            + "), 因此未建立连接");
        return std::nullopt;
    }

    int nowTime = ANowTime(time.wave);
    if (nowTime >= time.time) { // 现在要立即运行的操作
        _PrintLog(time, nowTime);
        timeOp.operation();
        return std::nullopt; // 无法控制这种操作的运行状态
    }

    auto&& queue = container[time.wave - 1].queue;
    auto ret = queue.emplace(time.time, std::move(timeOp));
    __aig.loggerPtr->Info("建立 时间-操作 [" + std::to_string(time.wave)
        + ", " + std::to_string(time.time) + "] 连接成功");
    return ret;
}

void __AOpQueueManager::RunOperation()
{
    int nowTime = AGetMainObject()->GameClock();
    auto totalWave = container.size();
    for (int wave = 1; wave <= totalWave; ++wave) {
        auto&& opQueue = container[wave - 1];
        if (opQueue.calRefreshTime == __AOperationQueue::UNINIT || opQueue.queue.empty()) { // 本波还未到达或者本波没有操作
            continue;
        }

        for (auto iter = opQueue.queue.begin(); iter != opQueue.queue.end(); iter = opQueue.queue.erase(iter)) {
            int queueNowTime = nowTime - opQueue.calRefreshTime;
            if (iter->first > queueNowTime) { // 时间未到达
                break;
            }

            if (iter->second.isStopped) { // 此时间连接已经被废除
                continue;
            }

            _PrintLog(ATime(wave, iter->first), queueNowTime);
            iter->second.operation();

            auto gameUi = AMPtr<APvzBase>(0x6a9ec0)->GameUi();
            if (gameUi != AAsm::PLAYING) {
                return; // 在非战斗界面需要立即退出
            }
        }
    }
}

void __AOpQueueManager::UpdateRefreshTime()
{
    auto mainObject = AGetMainObject();
    int wave = mainObject->Wave();
    if (wave == mainObject->TotalWave()) {
        return;
    }
    static int64_t runFlag = -1;
    auto gameClock = mainObject->GameClock();
    if (runFlag == gameClock) { // 保证此函数下面的内容一帧只会运行一次
        return;
    }
    runFlag = gameClock;

    auto operationQueueIter = container.begin() + wave;

    if (operationQueueIter->memRefreshTime != __AOperationQueue::UNINIT) { // 已经读取过的不再读取
        return;
    }

    int refreshCountdown = mainObject->RefreshCountdown();

    if (wave == 0) { // 第一波
        _SetRefreshTime(wave, refreshCountdown + gameClock);
        return;
    }

    if ((wave + 1) % 10 != 0) { // 普通波
        if (refreshCountdown <= 200) {
            _SetRefreshTime(wave, refreshCountdown + gameClock);
        }
    } else { // 大波
        if (refreshCountdown <= 200 && refreshCountdown > 5) {
            _SetRefreshTime(wave, refreshCountdown + 745 + gameClock);
        } else if (refreshCountdown <= 5) {
            _SetRefreshTime(wave, mainObject->HugeWaveCountdown() + gameClock);
        }
    }

    // 在 wave != 0 时, 可以由初始刷新倒计时得到当前已刷出波数的刷新时间点
    --operationQueueIter;
    if (operationQueueIter->memRefreshTime == __AOperationQueue::UNINIT) {
        _SetRefreshTime(wave - 1, gameClock - (mainObject->InitialCountdown() - refreshCountdown));
    }
}

void __AOpQueueManager::SetWavelength(const std::vector<ATime>& lst)
{
    for (const auto& time : lst) {
        if (time.wave == AGetMainObject()->TotalWave()) {
            __aig.loggerPtr->Error("SetWavelength 对最后一波无效");
            continue;
        }
        if (!_CheckWavelength(time)) {
            continue;
        }
        auto&& timeQueue = container[time.wave - 1];
        timeQueue.wavelength = time.time;
        if (timeQueue.calRefreshTime != __AOperationQueue::UNINIT) {
            container[time.wave].calRefreshTime = timeQueue.calRefreshTime + time.time;
        }
        auto setRefresh = [time]() {
            AGetMainObject()->ZombieRefreshHp() = -1;
            int countdown = (time.wave % 10 == 9 ? time.time - 745 : time.time);
            AGetMainObject()->RefreshCountdown() = countdown - 1;
            AGetMainObject()->InitialCountdown() = countdown;
        };
        AConnect(ATime(time.wave, 1), std::move(setRefresh));
    }
}

void __AOpQueueManager::AssumeWavelength(const std::vector<ATime>& lst)
{
    for (const auto& time : lst) {
        if (!_CheckWavelength(time)) {
            continue;
        }
        auto&& timeQueue = container[time.wave - 1];
        timeQueue.wavelength = time.time;
        if (timeQueue.calRefreshTime != __AOperationQueue::UNINIT) {
            container[time.wave].calRefreshTime = timeQueue.calRefreshTime + time.time;
        }

        // 本波的波长只有到了下波才知道
        AConnect(ATime(time.wave + 1, -200), [wave = time.wave, this] { _CheckAssumeWavelength(wave); });
    }
}

void __AOpQueueManager::_PrintLog(const ATime& time, int nowTime)
{
    if (nowTime > time.time) {
        __aig.loggerPtr->Warning("时间-操作 [" + std::to_string(time.wave) + ", " + std::to_string(time.time) + //
            "] 但是现在时间已到 [" + std::to_string(time.wave) + ", " + std::to_string(nowTime) + "]");
    }
    __aig.loggerPtr->Info("运行 时间-操作 [" + std::to_string(time.wave) + ", " + std::to_string(time.time) + "]");
}

bool __AOpQueueManager::_CheckWavelength(const ATime& time)
{
    if (time.wave == AGetMainObject()->TotalWave()) {
        return false;
    }

    if (time.time < (time.wave % 10 == 9 ? 1346 : 601)) {
        __aig.loggerPtr->Error(
            "您当前设定的第 " + std::to_string(time.wave)         //
            + " 波 的 time 参数为 " + std::to_string(time.time) + //
            ", 超出有效范围");
        return false;
    }

    if (container[time.wave - 1].wavelength != -1) {
        __aig.loggerPtr->Warning("您第 " + std::to_string(time.wave) + " 波已经设置过波长，此次设置无效");
        return false;
    }

    if (time.time > (time.wave % 10 == 9 ? 5245 : 3100)) {
        __aig.loggerPtr->Warning(
            "您当前设定的第 " + std::to_string(time.wave)         //
            + " 波 的 time 参数为 " + std::to_string(time.time) + //
            ", 超出有效范围");
    }

    return true;
}

void __AOpQueueManager::_CheckAssumeWavelength(int wave)
{
    auto&& currentTimeQueue = container[wave - 1];
    auto&& nextTimeQueue = container[wave];
    auto currentRefreshTime = currentTimeQueue.memRefreshTime;
    auto nextRefreshTime = nextTimeQueue.memRefreshTime;
    std::string str;
    if (nextRefreshTime == __AOperationQueue::UNINIT) { // 下波的实际时间还未到
        // 计算当前僵尸的血量
        int currentHp = AAsm::ZombieTotalHp(wave - 1);
        int refreshHp = AGetMainObject()->ZombieRefreshHp();
        int totalHp = AGetMainObject()->MRef<int>(0x5598);
        float refreshRatio = float(totalHp - currentHp) / (totalHp - refreshHp);

        str = "但下一波僵尸尚未刷新，目前僵尸总血量为 " + std::to_string(currentHp) //
            + " 刷新血量为 " + std::to_string(refreshHp)
            + " 刷新进度为 " + std::to_string(refreshRatio);
    } else {
        auto trueWavelength = nextRefreshTime - currentRefreshTime;
        if (trueWavelength != currentTimeQueue.wavelength) {
            str = "但实际波长为" + std::to_string(trueWavelength);
        }
    }

    if (!str.empty()) { // str 不为空说明有错误
        // 将已经设置的 calRefreshTime 设置回 UNINIT
        auto totalWave = container.size() - 2;
        for (int waveIter = wave; waveIter < totalWave; ++waveIter) {
            container[waveIter].calRefreshTime = __AOperationQueue::UNINIT;
        }

        __aig.loggerPtr->Error(
            "AssumeWavelength : 您第 "
            + std::to_string(wave) + " 波假定的波长为 "
            + std::to_string(currentTimeQueue.wavelength) + ", " + str);
    }
}

void __AOpQueueManager::_BeforeScript()
{
    container.clear();
    container.resize(AGetMainObject()->TotalWave());
    startTime = ATime(1, __AOperationQueue::UNINIT);
}

void __AOpQueueManager::_EnterFight()
{
    UpdateRefreshTime();
    startTime = ANowTime();
}

int ANowTime(int wave)
{
    if (AGetPvzBase()->GameUi() != 3) {
        return __AOperationQueue::UNINIT;
    }
    static int depth = 0;
    if (depth != 0) { // 此函数不能有递归
        return __AOperationQueue::UNINIT;
    }
    ++depth;
    auto maxWave = AGetMainObject()->TotalWave();
    if (wave <= 0 || wave > maxWave) {
        auto&& pattern = __aig.loggerPtr->GetPattern();
        // 此处会造成递归调用
        __aig.loggerPtr->Error("ANowTime 输入参数范围 [1, " + pattern + //
                "], 您输入的参数为 " + pattern + ", 已经超出了此范围",
            maxWave, wave);
        --depth;
        return __AOperationQueue::UNINIT;
    }
    if (__aOpQueueManager.container.size() < wave) {
        --depth;
        return __AOperationQueue::UNINIT;
    }
    auto refreshTime = __aOpQueueManager.container[wave - 1].calRefreshTime;
    --depth;
    return refreshTime == __AOperationQueue::UNINIT //
        ? __AOperationQueue::UNINIT
        : AGetMainObject()->GameClock() - refreshTime;
}
