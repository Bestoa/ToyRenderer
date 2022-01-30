#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>
#include "window.hpp"

using namespace std;
using namespace TGRenderer;

void TRWindow::__disp_func__(TRWindow *w)
{
    cout << "[TRWindow]: Start display thread." << endl;
    unique_lock<mutex> lck(w->mMutex);
    while (true)
    {
        w->mCV.wait(lck);
        if (!w->mRunning)
            break;
        if (!SDL_RenderCopy(w->mRenderer, w->mDispTexture, nullptr, nullptr))
            SDL_RenderPresent(w->mRenderer);
    }
    cout << "[TRWindow]: Stop display thread." << endl;
}

TRWindow::TRWindow(int w, int h, const char *name)
{
    mWidth = w;
    mHeight = h;

    if (SDL_Init(SDL_INIT_VIDEO))
        goto error;

    if (SDL_CreateWindowAndRenderer(mWidth, mHeight, 0, &mWindow, &mRenderer))
        goto error;
    SDL_SetWindowTitle(mWindow, name);

    mDispTexture = SDL_CreateTexture(mRenderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STREAMING, mWidth, mHeight);
    mDrawTexture = SDL_CreateTexture(mRenderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STREAMING, mWidth, mHeight);
    if (mDispTexture == nullptr || mDispTexture == nullptr)
        goto error;

    mBuffer = new TRBuffer(mWidth, mHeight, false);
    if (!mBuffer || !mBuffer->OK())
        goto error;

    void *addr;
    int pitch;
    if (SDL_LockTexture(mDrawTexture, nullptr, &addr, &pitch))
        goto error;
    assert(pitch == int(mBuffer->getStride() * BUFFER_CHANNEL));
    mBuffer->setExtBuffer(addr);
    trSetRenderTarget(mBuffer);

    mDisplayThread = new thread(__disp_func__, this);
    if (!mDisplayThread || !mDisplayThread->joinable())
        goto error;

    mRunning = true;

    return;

error:
    if (mBuffer)
        delete mBuffer;
    if (mDispTexture)
        SDL_DestroyTexture(mDispTexture);
    if (mDrawTexture)
        SDL_DestroyTexture(mDrawTexture);
    if (mRenderer)
        SDL_DestroyRenderer(mRenderer);
    if (mWindow)
        SDL_DestroyWindow(mWindow);

    SDL_Quit();
    mWidth = 0;
    mHeight = 0;
    mRunning = false;
}

bool TRWindow::isRunning() const
{
    return mRunning;
}


bool TRWindow::swapBuffer()
{
    unique_lock<mutex> lck(mMutex);
    SDL_UnlockTexture(mDrawTexture);
    std::swap(mDrawTexture, mDispTexture);
    mCV.notify_one();
    lck.unlock();

    void *addr;
    int pitch, ret;
    ret = SDL_LockTexture(mDrawTexture, nullptr, &addr, &pitch);
    mBuffer->setExtBuffer(addr);
    return ret;
};

bool TRWindow::shouldStop() const
{
    return mShouldStop;
}

void TRWindow::registerKeyEventCb(KeyEventCb func)
{
   mKcb = func;
}

void TRWindow::removeKeyEventCb()
{
   mKcb = nullptr;
}

void TRWindow::pollEvent()
{
    SDL_Event event;
    while (SDL_PollEvent(&event) != 0)
    {
        if (event.type == SDL_QUIT)
            mShouldStop = true;
        if (event.type == SDL_KEYDOWN) {
            if (event.key.keysym.scancode == SDL_SCANCODE_ESCAPE)
                mShouldStop = true;
            if (mKcb)
                mKcb(event.key.keysym.scancode);
        }
    }
}

TRWindow::~TRWindow()
{
    if (mDisplayThread && mDisplayThread->joinable())
    {
        unique_lock<mutex> lck(mMutex);
        mRunning = false;
        mCV.notify_one();
        // must unlock it manullay before join
        lck.unlock();
        mDisplayThread->join();
        cout << "[TRWindow]: Display thread exit." << endl;
        delete mDisplayThread;
    }
    delete mBuffer;
    SDL_DestroyTexture(mDispTexture);
    SDL_DestroyTexture(mDrawTexture);
    SDL_DestroyRenderer(mRenderer);
    SDL_DestroyWindow(mWindow);
    SDL_Quit();
}
