/*
 * @coding: utf-8
 * @Author: vector-wlc
 * @Date: 2020-02-06 10:22:46
 * @Description: old auto thread CLASS
 */

#include "avz_tick.h"
#include "avz_click.h"
#include "avz_memory.h"
#include "avz_card.h"

namespace AvZ
{
    extern std::vector<ThreadInfo> __thread_vec;
    extern std::stack<int> __stopped_thread_id_stack;

    extern MainObject *__main_object;
    extern PvZ *__pvz_base;
    extern HWND __pvz_hwnd;

    // *** Not In Queue
    // 通过得到线程的状态
    // *** 返回值：
    // 停止状态：return STOPED
    // 暂停状态：return PAUSED
    // 运行状态：return RUNNING
    TickRunner::ThreadStatus TickRunner::getStatus() const
    {
        if (thread_id < 0)
        {
            return STOPPED;
        }

        if (is_paused)
        {
            return PAUSED;
        }

        return RUNNING;
    }

    void TickRunner::pushFunc(const std::function<void()> &_run)
    {
        if (!thread_examine())
        {
            return;
        }
        is_paused = false;
        auto run = [=]() {
            if (is_paused)
            {
                return;
            }

            _run();
        };
        //如果没有找到停下来的线程则创建新线程
        if (__stopped_thread_id_stack.empty())
        {
            __thread_vec.push_back({run, &thread_id});
            thread_id = __thread_vec.size() - 1;
        }
        else
        {
            thread_id = __stopped_thread_id_stack.top();
            __thread_vec[thread_id] = {run, &thread_id};
            __stopped_thread_id_stack.pop();
        }
    }

    void ItemCollector::run()
    {
        if (__main_object->gameClock() % time_interval != 0 || __main_object->mouseAttribution()->type() != 0)
        {
            return;
        }

        auto item = __main_object->itemArray();
        for (int index = 0; index < __main_object->itemTotal(); ++index, ++item)
        {
            if (item->is_collected() || item->isDisappeared())
            {
                continue;
            }
            float item_x = item->abscissa();
            float item_y = item->ordinate();
            if (item_x >= 0.0 && item_y >= 70)
            {
                SafeClick();
                int x = static_cast<int>(item_x + 30);
                int y = static_cast<int>(item_y + 30);
                LeftClick(x, y);
                SafeClick();
                break;
            }
        }
    }

    ////////////////////////////////////////
    //  IceFiller
    ////////////////////////////////////////

    void IceFiller::resetIceSeedList(const std::vector<int> &lst)
    {
        // 未运行即进行检查是否为冰卡
        for (const auto &seed_type : lst)
        {
            if (seed_type != ICE_SHROOM && seed_type != M_ICE_SHROOM)
            {
                ShowErrorNotInQueue("resetIceSeedList : 您填写的参数为 # ，然而此函数只接受植物类型为 #（寒冰菇） 或 #（模仿寒冰菇）的参数", seed_type, ICE_SHROOM, M_ICE_SHROOM);
                return;
            }
        }

        InsertOperation([=]() {
            ice_seed_index_vec.clear();
            int ice_index = 0;
            for (const auto &seed_type : lst)
            {
                ice_index = GetSeedIndex(seed_type % 48, seed_type / 48);
                if (ice_index == -1)
                {
                    ShowErrorNotInQueue("resetIceSeedList : 您貌似没有选择对应的冰卡");
                    continue;
                }
                ice_seed_index_vec.push_back(ice_index);
            }
        },
                        "resetIceSeedList");
    }

    void IceFiller::start(const std::vector<Grid> &lst)
    {
        InsertOperation([=]() {
            ice_seed_index_vec.clear();
            int ice_seed_index;
            ice_seed_index = GetSeedIndex(ICE_SHROOM);
            if (ice_seed_index != -1)
            {
                ice_seed_index_vec.push_back(ice_seed_index);
            }
            ice_seed_index = GetSeedIndex(ICE_SHROOM, true);
            if (ice_seed_index != -1)
            {
                ice_seed_index_vec.push_back(ice_seed_index);
            }
            coffee_seed_index = GetSeedIndex(COFFEE_BEAN);
            fill_ice_grid_vec = lst;
            pushFunc([=]() { run(); });
        },
                        "startFillIce");
    }

    void IceFiller::run()
    {
        static auto plant = __main_object->plantArray();
        static auto seed = __main_object->seedArray();
        static std::vector<int> ice_plant_index_vec;
        static decltype(ice_plant_index_vec.begin()) ice_plant_index_it;
        static decltype(ice_seed_index_vec.begin()) ice_seed_index_it;
        static decltype(fill_ice_grid_vec.begin()) fill_ice_grid_it;
        static bool is_get_indexs = false;

        is_get_indexs = false;
        fill_ice_grid_it = fill_ice_grid_vec.begin();

        for (ice_seed_index_it = ice_seed_index_vec.begin();
             ice_seed_index_it != ice_seed_index_vec.end();
             ++ice_seed_index_it)
        {
            seed = __main_object->seedArray() + *ice_seed_index_it;
            if (!seed->isUsable())
            {
                continue;
            }
            if (!is_get_indexs)
            {
                GetPlantIndices(fill_ice_grid_vec, ICE_SHROOM, ice_plant_index_vec);
                ice_plant_index_it = ice_plant_index_vec.begin();
                is_get_indexs = true;
            }

            for (; ice_plant_index_it != ice_plant_index_vec.end();
                 ++fill_ice_grid_it, ++ice_plant_index_it)
            {
                if ((*ice_plant_index_it) == -1)
                {
                    //如果为池塘场景而且在水路
                    if ((__main_object->scene() == 2 || __main_object->scene() == 3) && (fill_ice_grid_it->row == 3 || fill_ice_grid_it->row == 4))
                    {
                        //如果不存在荷叶
                        if (GetPlantIndex(fill_ice_grid_it->row, fill_ice_grid_it->col, LILY_PAD) == -1)
                        {
                            continue;
                        }
                    }

                    // 如果是天台场景
                    if (__main_object->scene() == 4 || __main_object->scene() == 5)
                    {
                        if (GetPlantIndex(fill_ice_grid_it->row, fill_ice_grid_it->col, FLOWER_POT) == -1)
                        {
                            continue;
                        }
                    }

                    CardNotInQueue(*ice_seed_index_it + 1, fill_ice_grid_it->row, fill_ice_grid_it->col);
                    ++fill_ice_grid_it;
                    ++ice_plant_index_it;
                    break;
                }
            }
        }
    }

    void IceFiller::coffee()
    {
        InsertOperation([=]() {
            if (coffee_seed_index == -1)
            {
                ShowErrorNotInQueue("你没有选择咖啡豆卡片!");
                return;
            }

            if (fill_ice_grid_vec.empty())
            {
                ShowErrorNotInQueue("你还未为自动存冰对象初始化存冰列表");
                return;
            }
            std::vector<int> ice_plant_index_vec;
            GetPlantIndices(fill_ice_grid_vec, ICE_SHROOM, ice_plant_index_vec);

            auto fill_grid_it = fill_ice_grid_vec.end();
            do
            {
                --fill_grid_it;
                if (ice_plant_index_vec[fill_grid_it - fill_ice_grid_vec.begin()] > -1)
                {
                    SafeClick();
                    CardNotInQueue(coffee_seed_index + 1,
                                   fill_grid_it->row,
                                   fill_grid_it->col);
                    SafeClick();
                    return;
                }
            } while (fill_grid_it != fill_ice_grid_vec.begin());

            ShowErrorNotInQueue("coffee : 未找到可用的存冰");
        },
                        "coffee");
    }

    /////////////////////////////////////////////////
    //    PlantFixer
    /////////////////////////////////////////////////

    void PlantFixer::auto_get_fix_list()
    {
        grid_lst.clear();
        auto plant = __main_object->plantArray();
        int plant_cnt_max = __main_object->plantCountMax();
        Grid grid;
        for (int index = 0; index < plant_cnt_max; ++index, ++plant)
        {
            if (!plant->isCrushed() && !plant->isDisappeared() && plant->type() == plant_type)
            {
                grid.col = plant->col() + 1;
                grid.row = plant->row() + 1;
                grid_lst.push_back(grid);
            }
        }
    }

    void PlantFixer::autoGetFixList()
    {
        InsertOperation([=]() {
            auto_get_fix_list();
        },
                        "autoGetFixList");
    }

    void PlantFixer::use_seed_(int seed_index, int row, float col, bool is_need_shovel)
    {
        if (is_need_shovel)
        {
            ShovelNotInQueue(row, col, plant_type == PUMPKIN);
        }
        CardNotInQueue(seed_index + 1, row, col);
        if (is_use_coffee)
        {
            CardNotInQueue(coffee_seed_index + 1, row, col);
        }
    }

    void PlantFixer::get_seed_list()
    {
        seed_index_vec.clear();
        int seed_index;
        seed_index = GetSeedIndex(plant_type);
        if (-1 != seed_index)
        {
            seed_index_vec.push_back(seed_index);
        }
        seed_index = GetSeedIndex(plant_type, true);

        if (-1 != seed_index)
        {
            seed_index_vec.push_back(seed_index);
        }
        if (seed_index_vec.size() == 0)
        {
            ShowErrorNotInQueue("您没有选择修补该植物的卡片！");
        }
        coffee_seed_index = GetSeedIndex(COFFEE_BEAN);
    }

    void PlantFixer::start(int _plant_type, const std::vector<Grid> &lst, int _fix_hp)
    {
        if (_plant_type == COFFEE_BEAN)
        {
            ShowErrorNotInQueue("PlantFixer 不支持修补咖啡豆");
            return;
        }

        if (_plant_type >= GATLING_PEA)
        {
            ShowErrorNotInQueue("修补植物类仅支持绿卡");
            return;
        }

        InsertOperation([=]() {
            plant_type = _plant_type;
            fix_hp = _fix_hp;
            get_seed_list();
            //如果没有给出列表信息
            if (lst.size() == 0)
            {
                auto_get_fix_list();
            }
            else
            {
                grid_lst = lst;
            }
            pushFunc([=]() { run(); });
        },
                        "startFixPlant");
    }

    void PlantFixer::run()
    {
        static std::vector<int> plant_index_vec;
        static Grid need_plant_grid; //记录要使用植物的格子
        static int min_hp;           //记录要使用植物的格子
        static bool is_seed_used;    //种子是否被使用
        static decltype(seed_index_vec.begin()) usable_seed_index_it;
        static decltype(plant_index_vec.begin()) plant_index_it;
        static decltype(grid_lst.begin()) grid_it;

        usable_seed_index_it = seed_index_vec.begin();

        if (usable_seed_index_it == seed_index_vec.end())
        {
            return;
        }

        if (is_use_coffee)
        {
            if (coffee_seed_index == -1)
            {
                return;
            }
            auto coffee_seed = __main_object->seedArray() + coffee_seed_index;
            if (!coffee_seed->isUsable())
            {
                return;
            }
        }

        do
        {
            auto seed_memory = __main_object->seedArray();
            seed_memory += *usable_seed_index_it;
            if (seed_memory->isUsable())
            {
                break;
            }
            ++usable_seed_index_it;
        } while (usable_seed_index_it != seed_index_vec.end());

        // 没找到可用的卡片
        if (usable_seed_index_it == seed_index_vec.end())
        {
            return;
        }
        GetPlantIndices(grid_lst, plant_type, plant_index_vec);

        is_seed_used = false;
        need_plant_grid.row = need_plant_grid.col = 0; //格子信息置零
        min_hp = fix_hp;                               //最小生命值重置

        for (grid_it = grid_lst.begin(), plant_index_it = plant_index_vec.begin();
             grid_it != grid_lst.end();
             ++grid_it, ++plant_index_it)
        {
            //如果此处存在除植物类植物的植物
            if (*plant_index_it == -2)
            {
                continue;
            }

            if (*plant_index_it == -1)
            {
                //如果为池塘场景而且在水路
                if ((__main_object->scene() == 2 || __main_object->scene() == 3) && (grid_it->row == 3 || grid_it->row == 4))
                {
                    //如果不存在荷叶
                    if (GetPlantIndex(grid_it->row, grid_it->col, LILY_PAD) == -1 && plant_type != LILY_PAD)
                    {
                        continue;
                    }
                }

                // 如果是天台场景
                if (__main_object->scene() == 4 || __main_object->scene() == 5)
                {
                    if (GetPlantIndex(grid_it->row, grid_it->col, FLOWER_POT) == -1 && plant_type != FLOWER_POT)
                    {
                        continue;
                    }
                }

                use_seed_((*usable_seed_index_it), grid_it->row, grid_it->col, false);
                is_seed_used = true;
                break;
            }
            else
            {
                auto plant = __main_object->plantArray();
                plant += *plant_index_it;
                int plant_hp = plant->hp();
                //如果当前生命值低于最小生命值，记录下来此植物的信息
                if (plant_hp < min_hp)
                {
                    min_hp = plant_hp;
                    need_plant_grid.row = grid_it->row;
                    need_plant_grid.col = grid_it->col;
                }
            }
        }

        //如果有需要修补的植物且植物卡片能用则进行种植
        if (need_plant_grid.row && !is_seed_used)
        {
            //种植植物
            use_seed_((*usable_seed_index_it), need_plant_grid.row, need_plant_grid.col, true);
        }
    }

    /////////////////////////////////////////////////
    //    PlantFixer
    /////////////////////////////////////////////////

    void KeyConnector::add(char key, std::function<void()> operate)
    {
        if (key >= 'a' && key <= 'z')
            key -= 32;

        for (const auto &key_operation : key_operation_vec)
        {
            if (key_operation.first == key)
            {
                ShowErrorNotInQueue("按键 # 绑定了多个操作", key);
                return;
            }
        }

        key_operation_vec.push_back(std::pair<char, std::function<void()>>(key, operate));

        if (getStatus() == STOPPED)
        {
            pushFunc([=]() {
                for (const auto &key_operation : key_operation_vec)
                {
                    if ((GetAsyncKeyState(key_operation.first) & 0x8001) == 0x8001 && GetForegroundWindow() == __pvz_hwnd)
                    { // 检测 pvz 是否为顶层窗口
                        InsertGuard insert_guard(false);
                        key_operation.second();
                        return;
                    }
                }
            });
        }
    }
} // namespace AvZ