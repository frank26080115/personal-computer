

void btn_task(uint32_t now)
{
    static bool prev_btn1_down = false;
    static bool prev_btn2_down = false;
    static uint32_t ignore_time = 0;

    // this system will be powered on for months at a time, so implement a "infinit" time indicator
    if ((now - ignore_time) >= 0x0007FFFF) {
        ignore_time = 0;
    }

    if (BTN1_IS_DOWN() && BTN2_IS_DOWN() && (prev_btn1_down == false || prev_btn2_down == false))
    {
        btn3_flag = true;
        prev_btn1_down = true;
        prev_btn2_down = true;
        ignore_time = now;
        return;
    }

    if (BTN1_IS_DOWN())
    {
        if (prev_btn1_down == false && (ignore_time == 0 || (now - ignore_time) > 100))
        {
            btn1_flag = true;
            ignore_time = now;
        }

        prev_btn1_down = true;
    }
    else
    {
        prev_btn1_down = false;
    }

    if (BTN2_IS_DOWN())
    {
        if (prev_btn2_down == false && (ignore_time == 0 || (now - ignore_time) > 100))
        {
            btn2_flag = true;
            ignore_time = now;
        }

        prev_btn2_down = true;
    }
    else
    {
        prev_btn2_down = false;
    }
}