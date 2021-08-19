#pragma once


class EglContext
{
public:
    EglContext();
    ~EglContext();

    void    PrePaint();
    void    PostPaint();

    void    GetDisplaySize(int& Width,int& Height);
};

