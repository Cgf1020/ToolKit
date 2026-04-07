#pragma once

#include <QtWidgets/QWidget>
#include "ui_Widget.h"

#include <unordered_map>
#include <set>

class Widget : public QWidget
{
    Q_OBJECT

public:
    Widget(QWidget *parent = nullptr);
    ~Widget();

public:
    // 播放按钮点击事件
    void PushButtonClick();
    void PushButtonClick2();
    void PushButtonClick3();

    void TimerOnceStartPushButtonClick();
    void TimerOnceEndPushButtonClick();
    void TimerRepeatStartPushButtonClick();
    void TimerRepeatEndPushButtonClick();

private:
    void count();
    void count2(int i);
    void count3(std::string str, int l);

    void onTimerOnce(std::string timer_name);
    void onTimerRepeat(std::string timer_name);

private:
    Ui::WidgetClass ui;

    int _countnum{ 0 };

    //std::unordered_map<std::string, std::shared_ptr<itflee::Timer>> timer_map;
    std::set<std::string> timer_set;
};
