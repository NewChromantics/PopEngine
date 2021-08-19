#pragma once

#include <string>


namespace Egl
{
	void		IsOkay(const char* Context);
	std::string	GetString(int);//EGLint);
}


class EglContext
{
public:
    EglContext();
    ~EglContext();

    void    PrePaint();
    void    PostPaint();

    void    TestRender();
    void    GetDisplaySize(int& Width,int& Height);
};

