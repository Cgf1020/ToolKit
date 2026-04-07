#include "Widget.h"

#include <QDebug>
#include <QString>
#include <iostream>
#include <sstream>
#include "ToolKit/asyncallevent.h"
#include "ToolKit/timermanage.h"

static std::hash<std::thread::id> hasher;

Widget::Widget(QWidget *parent)
    : QWidget(parent)
{
    ui.setupUi(this);

    itflee::AsynCallEvent::instance()->Start();
    
    connect(ui.pushButton, &QPushButton::clicked, this, &Widget::PushButtonClick);
    connect(ui.pushButton_2, &QPushButton::clicked, this, &Widget::PushButtonClick2);
    connect(ui.pushButton_3, &QPushButton::clicked, this, &Widget::PushButtonClick3);

    connect(ui.timer_once_start, &QPushButton::clicked, this, &Widget::TimerOnceStartPushButtonClick);
    connect(ui.timer_once_end, &QPushButton::clicked, this, &Widget::TimerOnceEndPushButtonClick);
    connect(ui.timer_repeat_start, &QPushButton::clicked, this, &Widget::TimerRepeatStartPushButtonClick);
    connect(ui.timer_repeat_end, &QPushButton::clicked, this, &Widget::TimerRepeatEndPushButtonClick);
}

Widget::~Widget()
{}

void Widget::PushButtonClick()
{
    itflee::AsynCallEvent::instance()->Post(std::bind(&Widget::count, this));
}

void Widget::PushButtonClick2()
{
    itflee::AsynCallEvent::instance()->Post(std::bind(&Widget::count2, this, 2));
}

void Widget::PushButtonClick3()
{
    itflee::AsynCallEvent::instance()->Post(std::bind(&Widget::count3, this, "str", 5));
}

void Widget::TimerOnceStartPushButtonClick()
{
    qDebug() << "Once    Start";
	itflee::TimerManage::instance()->StartTimer("once", 5000, false, std::bind(&Widget::onTimerOnce, this, std::placeholders::_1));
	timer_set.insert("once");

    //auto timer = itflee::Timer::CreateTimer(5000, false,
    //    std::bind(&Widget::onTimerOnce, this, std::placeholders::_1));
    //timer_map["once"] = timer;
    //timer->StartTimer();
}

void Widget::TimerOnceEndPushButtonClick()
{
    qDebug() << "Once   End  1";

	qDebug() << timer_set.size();

    if (timer_set.find("once") != timer_set.end())
    {
        qDebug() << "Once    End 3";
		itflee::TimerManage::instance()->Invalidate("once");

		timer_set.erase("once");
    }    
    qDebug() << "Once   End  2";
}

void Widget::TimerRepeatStartPushButtonClick()
{
    qDebug() << "repeat    Start";
    itflee::TimerManage::instance()->StartTimer("repeat", 5000, true, std::bind(&Widget::onTimerOnce, this, std::placeholders::_1));
    timer_set.insert("repeat");
}

void Widget::TimerRepeatEndPushButtonClick()
{
	int i = 0;
    qDebug() << "repeat   End  1";

    qDebug() << timer_set.size();

    if (timer_set.find("repeat") != timer_set.end())
    {
        qDebug() << "repeat    End 3";
        itflee::TimerManage::instance()->Invalidate("repeat");

		timer_set.erase("repeat");
    }
    qDebug() << "repeat   End  2";
}

void Widget::count()
{
    _countnum++;

    static std::hash<std::thread::id> hasher;

    qDebug() << _countnum << " " << hasher(std::this_thread::get_id());

}

void Widget::count2(int i)
{
    qDebug() << i << " " << hasher(std::this_thread::get_id());
}

void Widget::count3(std::string str, int l)
{
    qDebug() << "count3" << " " << l << " " << hasher(std::this_thread::get_id());
}

void Widget::onTimerOnce(std::string timer_name)
{
    qDebug() << "onTimerOnce";

    if (timer_set.find(timer_name) != timer_set.end())
    {
        timer_set.erase(timer_name);
    }
}

void Widget::onTimerRepeat(std::string timer_name)
{
    qDebug() << "onTimerRepeat";
    if (timer_set.find(timer_name) != timer_set.end())
    {
        timer_set.erase(timer_name);
    }
}
