#include <windows.h>
LRESULT CALLBACK Proc(HWND w,UINT m,WPARAM a,LPARAM b){
 if(m==WM_PAINT){PAINTSTRUCT ps{};HDC dc=BeginPaint(w,&ps);RECT r;GetClientRect(w,&r);HBRUSH blue=CreateSolidBrush(RGB(20,110,210));FillRect(dc,&r,blue);DeleteObject(blue);r={60,50,560,300};HBRUSH gold=CreateSolidBrush(RGB(245,185,40));FillRect(dc,&r,gold);DeleteObject(gold);SetBkMode(dc,TRANSPARENT);SetTextColor(dc,RGB(255,255,255));TextOutW(dc,85,130,L"Spatial FX / Real Window Capture",32);EndPaint(w,&ps);return 0;}
 if(m==WM_TIMER){DestroyWindow(w);return 0;}if(m==WM_CLOSE){DestroyWindow(w);return 0;}if(m==WM_DESTROY){PostQuitMessage(0);return 0;}return DefWindowProcW(w,m,a,b);
}
int WINAPI wWinMain(HINSTANCE i,HINSTANCE,PWSTR,int){WNDCLASSW c{};c.lpfnWndProc=Proc;c.hInstance=i;c.lpszClassName=L"SpatialFXCaptureFixture";RegisterClassW(&c);auto w=CreateWindowExW(0,c.lpszClassName,L"Spatial FX Capture Fixture",WS_OVERLAPPEDWINDOW,80,80,660,420,nullptr,nullptr,i,nullptr);ShowWindow(w,SW_SHOW);ShowWindow(w,SW_SHOWNOACTIVATE);UpdateWindow(w);SetTimer(w,1,60000,nullptr);MSG m;while(GetMessageW(&m,nullptr,0,0)>0){TranslateMessage(&m);DispatchMessageW(&m);}return 0;}


