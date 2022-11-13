#include "klc_config.h"
#include "klc_defs.h"

void btn_task(uint32_t now)
{
    static bool prev_btn1_down = false;
    static bool prev_btn2_down = false;
    static bool need_both_release = false;
    static uint32_t ignore_time = 0;

    // this system will be powered on for months at a time, so implement a "infinit" time indicator
    if ((now - ignore_time) >= 0x0007FFFF) {
        ignore_time = 0;
    }

    if (BTNUPPER_IS_DOWN() && BTNLOWER_IS_DOWN() && (prev_btn1_down == false || prev_btn2_down == false))
    {
        btnboth_flag = true;
        prev_btn1_down = true;
        prev_btn2_down = true;
        ignore_time = now;
        need_both_release = true;
        return;
    }

    if (need_both_release)
    {
        if (BTNUPPER_IS_DOWN() == false && BTNLOWER_IS_DOWN() == false)
        {
            need_both_release = false;
            ignore_time = now;
            prev_btn1_down = false;
            prev_btn2_down = false;
        }
        return;
    }

    if (BTNUPPER_IS_DOWN())
    {
        prev_btn1_down = true;
    }
    else
    {
        if (prev_btn1_down != false && (ignore_time == 0 || (now - ignore_time) > 100))
        {
            btnupper_flag = true;
            ignore_time = now;
        }
        prev_btn1_down = false;
    }

    if (BTNLOWER_IS_DOWN())
    {
        prev_btn2_down = true;
    }
    else
    {
        if (prev_btn2_down != false && (ignore_time == 0 || (now - ignore_time) > 100))
        {
            btnlower_flag = true;
            ignore_time = now;
        }
        prev_btn2_down = false;
    }
}