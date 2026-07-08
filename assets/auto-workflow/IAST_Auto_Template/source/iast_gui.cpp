// iast_gui.cpp — C++ IAST Calculator GUI (Win32 + GDI+)
// Compile: cl /O2 /EHsc /std:c++17 /Fe:iast_calc_gui.exe iast_gui.cpp /link gdiplus.lib comctl32.lib
// Zero external dependencies beyond Windows SDK.

#ifndef UNICODE
#define UNICODE
#endif
#define NOMINMAX
#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>
#include <gdiplus.h>
#include <objidl.h>
#include <shellapi.h>
#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <sstream>
#include <fstream>
#include <cmath>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
using namespace Gdiplus;
using std::min; using std::max;
#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "comctl32.lib")

#include "iast_core.h"
#define main run_iast_cli
#include "iast_cli.cpp"
#undef main
using namespace iast;

// ─── Globals ───────────────────────────────────────────────
HINSTANCE g_hInst;
HWND g_hMain, g_hLog, g_hChart, g_hChartCmb;
HWND g_hCsvPaths[3], g_hModelCmb[3], g_hParams[3];
HWND g_hY[3], g_hPressures, g_hPrefix, g_hRunBtn;
int g_numGases = 2;
int g_activeChart = 0;
std::vector<double> g_chartX[4], g_chartY[4][3];
std::string g_chartTitle[4], g_chartXLabel[4], g_chartYLabel[4];
std::string g_chartLegends[4][3];
int g_chartSeriesCount[4] = {0};
std::mutex g_resultMutex;
bool g_calculating = false;
std::string g_logText;

// ─── GDI+ Init ────────────────────────────────────────────
ULONG_PTR g_gdiToken;
void InitGDIPlus() { GdiplusStartupInput inp; GdiplusStartup(&g_gdiToken, &inp, NULL); }
void ShutdownGDIPlus() { GdiplusShutdown(g_gdiToken); }

// ─── Chart Drawing ────────────────────────────────────────
void DrawChart(HWND, HDC hdc, RECT rc) {
    int idx = g_activeChart;
    int w = rc.right - rc.left, h = rc.bottom - rc.top;
    HDC memDC = CreateCompatibleDC(hdc);
    HBITMAP memBmp = CreateCompatibleBitmap(hdc, w, h);
    SelectObject(memDC, memBmp);
    Graphics g(memDC);
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    g.Clear(Color(255, 255, 255, 255));

    if (g_chartX[idx].empty() || g_chartSeriesCount[idx] == 0) {
        SolidBrush br(Color(150,150,150)); Font f(L"Segoe UI",12);
        g.DrawString(L"No data yet",-1,&f,PointF(rc.right/2.0f-50,rc.bottom/2.0f-10),&br);
    } else {
        double xmin=1e30,xmax=-1e30,ymin=1e30,ymax=-1e30;
        for(int s=0;s<g_chartSeriesCount[idx];++s){
            for(double v:g_chartX[idx]){xmin=min(xmin,v);xmax=max(xmax,v);}
            for(double v:g_chartY[idx][s]){ymin=min(ymin,v);ymax=max(ymax,v);}
        }
        double xp=max((xmax-xmin)*0.08,1.0), yp=max((ymax-ymin)*0.08,0.02);
        xmin-=xp;xmax+=xp;ymin-=yp;ymax+=yp;
        if(xmin==xmax){xmin-=1;xmax+=1;} if(ymin==ymax){ymin-=1;ymax+=1;}
        int ml=65,mr=20,mt=35,mb=50;
        double sx=(w-ml-mr)/(xmax-xmin), sy=(h-mt-mb)/(ymax-ymin);
        auto tx=[&](double v){return(float)(ml+(v-xmin)*sx);};
        auto ty=[&](double v){return(float)(h-mb-(v-ymin)*sy);};
        Pen gridPen(Color(220,220,220),0.5f);
        for(int i=0;i<=5;++i){double v=xmin+(xmax-xmin)*i/5.0;g.DrawLine(&gridPen,(REAL)tx(v),(REAL)mt,(REAL)tx(v),(REAL)(h-mb));}
        for(int i=0;i<=5;++i){double v=ymin+(ymax-ymin)*i/5.0;g.DrawLine(&gridPen,(REAL)ml,(REAL)ty(v),(REAL)(w-mr),(REAL)ty(v));}
        Pen axisPen(Color(80,80,80),1.5f);
        g.DrawLine(&axisPen,(REAL)ml,(REAL)ty(ymin),(REAL)ml,(REAL)ty(ymax));
        g.DrawLine(&axisPen,(REAL)tx(xmin),(REAL)ty(ymin),(REAL)tx(xmax),(REAL)ty(ymin));
        Color colors[3]={Color(46,204,113),Color(231,76,60),Color(52,152,219)};
        for(int s=0;s<g_chartSeriesCount[idx];++s){
            SolidBrush br(colors[s]);
            for(size_t i=0;i<g_chartX[idx].size();++i)
                g.FillEllipse(&br,(REAL)(tx(g_chartX[idx][i])-3),(REAL)(ty(g_chartY[idx][s][i])-3),(REAL)7,(REAL)7);
        }
        Font tickF(L"Segoe UI",9); SolidBrush tickBr(Color(80,80,80));
        StringFormat sf; sf.SetAlignment(StringAlignmentCenter);
        wchar_t buf[32];
        for(int i=0;i<=5;++i){double v=xmin+(xmax-xmin)*i/5.0;swprintf(buf,32,L"%.4g",v);
            g.DrawString(buf,-1,&tickF,PointF(tx(v),h-mb+5),&sf,&tickBr);}
        sf.SetAlignment(StringAlignmentFar);
        for(int i=0;i<=5;++i){double v=ymin+(ymax-ymin)*i/5.0;swprintf(buf,32,L"%.4g",v);
            g.DrawString(buf,-1,&tickF,PointF(ml-5,ty(v)-7),&sf,&tickBr);}
        Font labelF(L"Segoe UI",11); SolidBrush labelBr(Color(20,20,20));
        sf.SetAlignment(StringAlignmentCenter);
        int wbuf[256];
        MultiByteToWideChar(CP_UTF8,0,g_chartXLabel[idx].c_str(),-1,(LPWSTR)wbuf,256);
        g.DrawString((LPWSTR)wbuf,-1,&labelF,PointF(w/2.0f,h-12),&sf,&labelBr);
        MultiByteToWideChar(CP_UTF8,0,g_chartYLabel[idx].c_str(),-1,(LPWSTR)wbuf,256);
        g.TranslateTransform(14,h/2.0f);g.RotateTransform(-90);
        g.DrawString((LPWSTR)wbuf,-1,&labelF,PointF(0,0),&sf,&labelBr);g.ResetTransform();
        Font titleF(L"Segoe UI",12,FontStyleBold);
        MultiByteToWideChar(CP_UTF8,0,g_chartTitle[idx].c_str(),-1,(LPWSTR)wbuf,256);
        g.DrawString((LPWSTR)wbuf,-1,&titleF,PointF(w/2.0f,5),&sf,&labelBr);
        float ly=40; Font legF(L"Segoe UI",10);
        for(int s=0;s<g_chartSeriesCount[idx];++s){
            SolidBrush legBr(colors[s]);
            g.FillRectangle(&legBr,(REAL)(w-130),(REAL)ly,(REAL)10,(REAL)10);
            MultiByteToWideChar(CP_UTF8,0,g_chartLegends[idx][s].c_str(),-1,(LPWSTR)wbuf,256);
            g.DrawString((LPWSTR)wbuf,-1,&legF,PointF(w-115,ly-1),&labelBr);ly+=18;
        }
    }
    BitBlt(hdc,0,0,w,h,memDC,0,0,SRCCOPY);
    DeleteObject(memBmp); DeleteDC(memDC);
}

// ─── Helpers ───────────────────────────────────────────────
std::string GetEditText(HWND h){int l=GetWindowTextLengthA(h);std::string s(l,0);GetWindowTextA(h,&s[0],l+1);return s;}
std::vector<double> ParseInitParams(const std::string& s){
    std::vector<double> p={1,1,1,1,1,1}; if(s.empty())return p;
    std::stringstream ss(s);std::string t;
    while(std::getline(ss,t,',')){size_t e=t.find('=');if(e==std::string::npos)continue;
        std::string k=t.substr(0,e);double v=std::stod(t.substr(e+1));
        if(k=="A1")p[0]=v;else if(k=="B1")p[1]=v;else if(k=="C1")p[2]=v;
        else if(k=="A2")p[3]=v;else if(k=="B2")p[4]=v;else if(k=="C2")p[5]=v;}
    return p;
}
int GetEncoderClsid(const WCHAR* fmt,CLSID* clsid){
    unsigned n=0,sz=0;GetImageEncodersSize(&n,&sz);if(!sz)return-1;
    ImageCodecInfo* ic=(ImageCodecInfo*)malloc(sz);GetImageEncoders(n,sz,ic);
    for(unsigned j=0;j<n;++j){if(wcscmp(ic[j].MimeType,fmt)==0){*clsid=ic[j].Clsid;free(ic);return j;}}
    free(ic);return-1;
}

// ─── Save Chart Image ─────────────────────────────────────
void SaveChartImage(HWND ow){
    RECT rc;GetClientRect(g_hChart,&rc);
    int w=rc.right-rc.left,h=rc.bottom-rc.top;if(w<=0||h<=0)return;
    HDC sc=GetDC(g_hChart);HDC mc=CreateCompatibleDC(sc);
    HBITMAP hb=CreateCompatibleBitmap(sc,w,h);HBITMAP ob=(HBITMAP)SelectObject(mc,hb);
    DrawChart(g_hChart,mc,rc);
    Bitmap* bmp=Bitmap::FromHBITMAP(hb,NULL);
    SelectObject(mc,ob);DeleteObject(hb);DeleteDC(mc);ReleaseDC(g_hChart,sc);
    wchar_t bf[MAX_PATH]={0};
    OPENFILENAMEW ofn={sizeof(ofn)};ofn.hwndOwner=ow;
    ofn.lpstrFilter=L"PNG\0*.png\0BMP\0*.bmp\0";ofn.lpstrFile=bf;ofn.nMaxFile=MAX_PATH;
    ofn.lpstrDefExt=L"png";ofn.Flags=OFN_OVERWRITEPROMPT|OFN_HIDEREADONLY;
    if(!GetSaveFileNameW(&ofn)){delete bmp;return;}
    std::string ext;for(wchar_t c:bf)ext+=(char)tolower(c);
    const WCHAR* mime=(ext.find(".png")!=std::string::npos)?L"image/png":L"image/bmp";
    CLSID clsid;if(GetEncoderClsid(mime,&clsid)>=0)bmp->Save(bf,&clsid,NULL);
    delete bmp;
}

// ─── Save Log ─────────────────────────────────────────────
void SaveLog(HWND ow){
    if(g_logText.empty()){MessageBoxA(ow,"Log is empty.","Info",MB_OK|MB_ICONINFORMATION);return;}
    wchar_t bf[MAX_PATH]={0};
    OPENFILENAMEW ofn={sizeof(ofn)};ofn.hwndOwner=ow;
    ofn.lpstrFilter=L"Text\0*.txt\0";ofn.lpstrFile=bf;ofn.nMaxFile=MAX_PATH;
    ofn.lpstrDefExt=L"txt";ofn.Flags=OFN_OVERWRITEPROMPT|OFN_HIDEREADONLY;
    if(!GetSaveFileNameW(&ofn))return;
    HANDLE hf=CreateFileW(bf,GENERIC_WRITE,0,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);
    if(hf!=INVALID_HANDLE_VALUE){
        // Write UTF-8 BOM then content
        unsigned char bom[]={0xEF,0xBB,0xBF};
        DWORD wr;WriteFile(hf,bom,3,&wr,NULL);
        WriteFile(hf,g_logText.c_str(),(DWORD)g_logText.size(),&wr,NULL);
        CloseHandle(hf);
    }
}

// ─── Calc Thread ──────────────────────────────────────────
DWORD WINAPI CalcThread(LPVOID){
    g_calculating=true;
    auto AL=[](const std::string&s){std::lock_guard<std::mutex>lk(g_resultMutex);g_logText+=s+"\r\n";};
    try{
        std::vector<std::string> names,csvs,models;std::vector<std::vector<double>> inits;
        for(int i=0;i<g_numGases;++i){
            names.push_back("Component " + std::to_string(i+1));csvs.push_back(GetEditText(g_hCsvPaths[i]));
            int sel=(int)SendMessage(g_hModelCmb[i],CB_GETCURSEL,0,0);
            const char*mn[]={"Auto","SSL","DSL","SSLF","DSLF"};models.push_back(mn[max(0,min(4,sel))]);
            inits.push_back(ParseInitParams(GetEditText(g_hParams[i])));
        }
        std::vector<double> y;double ys=0;
        for(int i=0;i<g_numGases;++i){double yi=atof(GetEditText(g_hY[i]).c_str());y.push_back(yi);ys+=yi;}
        if(abs(ys-1.0)>0.01){ys=0;for(int i=0;i<g_numGases-1;++i)ys+=y[i];y.back()=1.0-ys;}
        std::string ps=GetEditText(g_hPressures);std::vector<double> pressures;
        if(ps=="auto"||ps.empty()){for(int i=1;i<=10;++i)pressures.push_back(i);for(int i=20;i<=100;i+=10)pressures.push_back(i);pressures.push_back(105.0);}
        else{std::stringstream ss(ps);std::string t;while(std::getline(ss,t,','))if(!t.empty())pressures.push_back(atof(t.c_str()));}
        std::filesystem::path csvDir=std::filesystem::path(csvs.front()).parent_path();
        set_output_base_dir(csvDir);
        std::string prefix=GetEditText(g_hPrefix);if(prefix.empty())prefix="IAST_result";
        prefix=final_output_prefix(prefix);
        AL("=== IAST Calculation ===");
        AL("Prefix: "+prefix);
        {
            std::string mx="Mixture:";for(size_t i=0;i<names.size();++i){if(i)mx+=" + ";mx+=names[i];}AL(mx);
            AL("Gas fractions (y):");for(int i=0;i<g_numGases;++i){char bf[64];snprintf(bf,64,"  %s = %.6f",names[i].c_str(),y[i]);AL(bf);}
            AL("Model & initial params:");
            for(int i=0;i<g_numGases;++i){
                char bf[256];std::string ip=GetEditText(g_hParams[i]);
                snprintf(bf,256,"  %s: %s  init=(%s)  CSV=%s",names[i].c_str(),models[i].c_str(),ip.c_str(),csvs[i].c_str());AL(bf);
            }
            AL("Pressures (kPa): "+ps);
        }
        std::vector<std::vector<double>> gasP,gasU;
        for(int i=0;i<g_numGases;++i){auto[P,U]=read_isotherm_csv(csvs[i]);gasP.push_back(P);gasU.push_back(U);
            AL(names[i]+": "+std::to_string(P.size())+" data pts");}
        AL("Fitting isotherm models...");std::vector<std::vector<double>> fp;std::vector<std::string> selectedModels;
        for(int i=0;i<g_numGases;++i){char bf[512];
            auto fit=fit_model_auto(gasP[i],gasU[i],models[i],inits[i]);auto res=fit.result;fp.push_back(res.params);selectedModels.push_back(fit.selected_model);
            auto fn=make_model_fn(fit.selected_model);
            auto errors=compute_param_errors(gasP[i],gasU[i],fn,res.params);
            snprintf(bf,512,"  %s requested=%s selected=%s: R^2=%.6f chi^2=%.6f BIC=%.6f iter=%d ok=%d",names[i].c_str(),models[i].c_str(),fit.selected_model.c_str(),res.r_squared,res.chi_squared,bic_score(res.chi_squared,(int)gasU[i].size(),active_param_count(fit.selected_model)),res.iterations,res.converged);AL(bf);
            if(fit.candidates.size()>1){
                AL("    Candidate models:");
                for(auto&c:fit.candidates){char cb[256];snprintf(cb,256,"      %s: R^2=%.6f chi^2=%.6f BIC=%.6f",c.model.c_str(),c.result.r_squared,c.result.chi_squared,c.score);AL(cb);}
            }
            std::string ps="    Fitted params: ";for(size_t k=0;k<res.params.size();++k){if(k)ps+=", ";char nb[32];snprintf(nb,32,"%+.6g",res.params[k]);ps+=nb;}AL(ps);
            if(!errors.empty()){
                std::string es="    Std Errors:    ";for(size_t k=0;k<errors.size();++k){if(k)es+=", ";char nb[32];snprintf(nb,32,"%.3g",errors[k]);es+=nb;}AL(es);
            }
        }
        AL("Running IAST...");std::string csvPath=prefix+"_Selectivity.csv";
        std::string resultLog;
        {
            std::vector<double> po,u1,u2,sel,sp,u3;
            if(g_numGases==2){
                auto r=binary_iast(y[0],pressures,fp[0],fp[1]);
                std::vector<std::string> hdr={"P(kPa)",names[0]+" Up",names[1]+" Up",names[0]+" x",names[1]+" x","S","SP"};
                std::vector<std::vector<double>> rows;
                for(auto&ri:r){rows.push_back({ri.P,ri.uptake1,ri.uptake2,ri.x1,ri.x2,ri.selectivity,ri.sep_potential});
                    po.push_back(ri.P);u1.push_back(ri.uptake1);u2.push_back(ri.uptake2);sel.push_back(ri.selectivity);sp.push_back(ri.sep_potential);}
                write_csv(csvPath,hdr,rows);
                // Build result table for log
                resultLog="IAST Binary Results:\r\n";
                resultLog+="  P(kPa)  "+names[0]+"_Upt  "+names[1]+"_Upt  "+names[0]+"_x    "+names[1]+"_x    Sel    SepPot\r\n";
                for(auto&ri:r){char bf[200];snprintf(bf,200,"  %6.2f  %8.4f  %8.4f  %6.4f  %6.4f  %6.2f  %8.4f",ri.P,ri.uptake1,ri.uptake2,ri.x1,ri.x2,ri.selectivity,ri.sep_potential);resultLog+=bf;resultLog+="\r\n";}
                // Lock for chart data
                std::lock_guard<std::mutex>lk(g_resultMutex);
                g_chartSeriesCount[0]=g_numGases;g_chartX[0]=gasP[0];
                for(int i=0;i<g_numGases;++i)g_chartY[0][i]=gasU[i];
                g_chartTitle[0]="Pure-component adsorption isotherms";g_chartXLabel[0]="Pressure (kPa)";g_chartYLabel[0]="Gas uptake (mmol/g)";
                for(int i=0;i<g_numGases;++i)g_chartLegends[0][i]=names[i];
                g_chartSeriesCount[1]=2;g_chartX[1]=po;g_chartY[1][0]=u1;g_chartY[1][1]=u2;
                g_chartTitle[1]=names[0]+" and "+names[1]+" mixed-gas sorption";g_chartXLabel[1]="Total pressure (kPa)";g_chartYLabel[1]="Gas uptake (mmol/g)";
                g_chartLegends[1][0]=names[0]+" IAST";g_chartLegends[1][1]=names[1]+" IAST";
                g_chartSeriesCount[2]=1;g_chartX[2]=po;g_chartY[2][0]=sel;
                g_chartTitle[2]="Selectivity "+names[0]+":"+names[1];g_chartXLabel[2]="Total pressure (kPa)";g_chartYLabel[2]="Selectivity";g_chartLegends[2][0]="IAST selectivity";
                g_chartSeriesCount[3]=1;g_chartX[3]=po;g_chartY[3][0]=sp;
                g_chartTitle[3]="Separation Potential - "+names[0]+"/"+names[1];g_chartXLabel[3]="Total pressure (kPa)";g_chartYLabel[3]="Separation Potential (mmol/g)";g_chartLegends[3][0]="Separation Potential";
                std::vector<std::pair<std::string,std::pair<std::vector<double>,std::vector<double>>>> pureSeries;
                for(int i=0;i<g_numGases;++i)pureSeries.push_back({names[i],{gasP[i],gasU[i]}});
                svg_scatter(prefix+"_Exp_isotherms.svg",pureSeries,"Pressure (kPa)","Gas uptake (mmol/g)","Pure-component adsorption isotherms");
                svg_scatter(prefix+"_IAST_validation.svg",{{names[0]+" IAST",{po,u1}},{names[1]+" IAST",{po,u2}}},"Total pressure (kPa)","Gas uptake (mmol/g)",names[0]+" and "+names[1]+" mixed-gas sorption");
                svg_scatter(prefix+"_Selectivity.svg",{{"IAST selectivity",{po,sel}}},"Total pressure (kPa)","Selectivity","Selectivity "+names[0]+":"+names[1]);
                svg_scatter(prefix+"_Separation_Potential.svg",{{"Separation Potential",{po,sp}}},"Total pressure (kPa)","Separation Potential","Separation Potential - "+names[0]+"/"+names[1]);
            }else{
                auto r=ternary_iast(y,pressures,fp[0],fp[1],fp[2]);
                std::vector<std::string> hdr={"P(kPa)",names[0]+" Up",names[1]+" Up",names[2]+" Up",names[0]+" x",names[1]+" x",names[2]+" x","S"};
                std::vector<std::vector<double>> rows;
                for(auto&ri:r){rows.push_back({ri.P,ri.uptake1,ri.uptake2,ri.uptake3,ri.x1,ri.x2,ri.x3,ri.selectivity});
                    po.push_back(ri.P);u1.push_back(ri.uptake1);u2.push_back(ri.uptake2);u3.push_back(ri.uptake3);sel.push_back(ri.selectivity);}
                write_csv(csvPath,hdr,rows);
                // Build result table for log
                resultLog="IAST Ternary Results:\r\n";
                resultLog+="  P(kPa)  "+names[0]+"_Upt  "+names[1]+"_Upt  "+names[2]+"_Upt  "+names[0]+"_x    "+names[1]+"_x    "+names[2]+"_x    Sel\r\n";
                for(auto&ri:r){char bf[250];snprintf(bf,250,"  %6.2f  %8.4f  %8.4f  %8.4f  %6.4f  %6.4f  %6.4f  %6.2f",ri.P,ri.uptake1,ri.uptake2,ri.uptake3,ri.x1,ri.x2,ri.x3,ri.selectivity);resultLog+=bf;resultLog+="\r\n";}
                // Lock for chart data
                std::lock_guard<std::mutex>lk(g_resultMutex);
                g_chartSeriesCount[0]=g_numGases;g_chartX[0]=gasP[0];
                for(int i=0;i<g_numGases;++i)g_chartY[0][i]=gasU[i];
                g_chartTitle[0]="Pure-component adsorption isotherms";g_chartXLabel[0]="Pressure (kPa)";g_chartYLabel[0]="Gas uptake (mmol/g)";
                for(int i=0;i<g_numGases;++i)g_chartLegends[0][i]=names[i];
                g_chartSeriesCount[1]=3;g_chartX[1]=po;g_chartY[1][0]=u1;g_chartY[1][1]=u2;g_chartY[1][2]=u3;
                g_chartTitle[1]=names[0]+", "+names[1]+", "+names[2]+" mixed-gas sorption";g_chartXLabel[1]="Total pressure (kPa)";g_chartYLabel[1]="Gas uptake (mmol/g)";
                for(int i=0;i<3;++i)g_chartLegends[1][i]=names[i]+" IAST";
                g_chartSeriesCount[2]=1;g_chartX[2]=po;g_chartY[2][0]=sel;
                g_chartTitle[2]="Selectivity";g_chartXLabel[2]="Total pressure (kPa)";g_chartYLabel[2]="Selectivity";
                g_chartSeriesCount[3]=0;
                std::vector<std::pair<std::string,std::pair<std::vector<double>,std::vector<double>>>> pureSeries;
                for(int i=0;i<g_numGases;++i)pureSeries.push_back({names[i],{gasP[i],gasU[i]}});
                svg_scatter(prefix+"_Exp_isotherms.svg",pureSeries,"Pressure (kPa)","Gas uptake (mmol/g)","Pure-component adsorption isotherms");
                svg_scatter(prefix+"_IAST_validation.svg",{{names[0]+" IAST",{po,u1}},{names[1]+" IAST",{po,u2}},{names[2]+" IAST",{po,u3}}},"Total pressure (kPa)","Gas uptake (mmol/g)",names[0]+", "+names[1]+", "+names[2]+" mixed-gas sorption");
                svg_scatter(prefix+"_Selectivity.svg",{{"IAST selectivity",{po,sel}}},"Total pressure (kPa)","Selectivity","Selectivity");
            }
        }
        if(!resultLog.empty()){std::lock_guard<std::mutex>lk(g_resultMutex);g_logText+=resultLog;}
        AL("Output files:");
        AL("  "+csvPath);
        AL("  "+prefix+"_Exp_isotherms.svg");
        AL("  "+prefix+"_IAST_validation.svg");
        AL("  "+prefix+"_Selectivity.svg");
        if(g_numGases==2)AL("  "+prefix+"_Separation_Potential.svg");
        AL("=== Done ===");
        // Auto-save log
        {
            std::string logPath = prefix + "_log.txt";
            std::wstring wpath(logPath.begin(), logPath.end());
            HANDLE hf = CreateFileW(wpath.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
            if (hf != INVALID_HANDLE_VALUE) {
                unsigned char bom[] = {0xEF, 0xBB, 0xBF};
                DWORD wr; WriteFile(hf, bom, 3, &wr, NULL);
                WriteFile(hf, g_logText.c_str(), (DWORD)g_logText.size(), &wr, NULL);
                CloseHandle(hf);
            }
        }
    }catch(std::exception&e){AL(std::string("ERROR: ")+e.what());}
    g_calculating=false;PostMessage(g_hMain,WM_USER+1,0,0);return 0;
}

// ─── Window Procedures ─────────────────────────────────────
LRESULT CALLBACK ChartWndProc(HWND hwnd, UINT msg, WPARAM, LPARAM lp){
    switch(msg){
        case WM_PAINT:{PAINTSTRUCT ps;HDC hdc=BeginPaint(hwnd,&ps);RECT rc;GetClientRect(hwnd,&rc);DrawChart(hwnd,hdc,rc);EndPaint(hwnd,&ps);return 0;}
        case WM_ERASEBKGND: return 1;
    }
    return DefWindowProc(hwnd,msg,0,lp);
}

void RefreshChart(){InvalidateRect(g_hChart,NULL,TRUE);}

LRESULT CALLBACK MainWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp){
    switch(msg){
        case WM_CREATE:{
            HINSTANCE hi=((LPCREATESTRUCT)lp)->hInstance;
            g_hMain=hwnd;
            INITCOMMONCONTROLSEX icex={sizeof(icex),ICC_STANDARD_CLASSES};InitCommonControlsEx(&icex);
            WNDCLASSEXW wc={sizeof(wc)};wc.lpfnWndProc=ChartWndProc;wc.hInstance=hi;
            wc.hCursor=LoadCursor(NULL,IDC_ARROW);wc.hbrBackground=(HBRUSH)(COLOR_WINDOW+1);
            wc.lpszClassName=L"IastChart";RegisterClassExW(&wc);
            int yPos=5;
            for(int i=0;i<3;++i){
                CreateWindowA("STATIC",i==0?"Gas1:":(i==1?"Gas2:":"Gas3:"),WS_CHILD|WS_VISIBLE,10,yPos+2,35,20,hwnd,(HMENU)(400+i),hi,NULL);
                g_hCsvPaths[i]=CreateWindowA("EDIT","",WS_CHILD|WS_VISIBLE|WS_BORDER|ES_AUTOHSCROLL,45,yPos,190,22,hwnd,NULL,hi,NULL);
                CreateWindowA("BUTTON","...",WS_CHILD|WS_VISIBLE,237,yPos,25,22,hwnd,(HMENU)(100+i),hi,NULL);
                CreateWindowA("STATIC","Model:",WS_CHILD|WS_VISIBLE,267,yPos+2,35,20,hwnd,(HMENU)(410+i),hi,NULL);
                g_hModelCmb[i]=CreateWindowA("COMBOBOX","",WS_CHILD|WS_VISIBLE|CBS_DROPDOWNLIST,304,yPos,60,130,hwnd,NULL,hi,NULL);
                SendMessageA(g_hModelCmb[i],CB_ADDSTRING,0,(LPARAM)"Auto");
                SendMessageA(g_hModelCmb[i],CB_ADDSTRING,0,(LPARAM)"SSL");SendMessageA(g_hModelCmb[i],CB_ADDSTRING,0,(LPARAM)"DSL");
                SendMessageA(g_hModelCmb[i],CB_ADDSTRING,0,(LPARAM)"SSLF");SendMessageA(g_hModelCmb[i],CB_ADDSTRING,0,(LPARAM)"DSLF");
                SendMessage(g_hModelCmb[i],CB_SETCURSEL,0,0);
                g_hParams[i]=CreateWindowA("EDIT","A1=1.0",WS_CHILD|WS_VISIBLE|WS_BORDER|ES_AUTOHSCROLL,367,yPos,200,22,hwnd,NULL,hi,NULL);
                yPos+=26;
            }
            ShowWindow(g_hCsvPaths[2],SW_HIDE);
            ShowWindow(g_hModelCmb[2],SW_HIDE);ShowWindow(g_hParams[2],SW_HIDE);
            yPos+=3;
            for(int i=0;i<3;++i){
                char ylbl[16];snprintf(ylbl,16,"G%d y:",i+1);
                CreateWindowA("STATIC",ylbl,WS_CHILD|WS_VISIBLE,10+i*75,yPos+2,32,20,hwnd,(HMENU)(430+i),hi,NULL);
                g_hY[i]=CreateWindowA("EDIT",i<2?"0.5":"0.0",WS_CHILD|WS_VISIBLE|WS_BORDER|ES_AUTOHSCROLL,44+i*75,yPos,28,22,hwnd,NULL,hi,NULL);
                if(i==2){ShowWindow(GetDlgItem(hwnd,430+i),SW_HIDE);ShowWindow(g_hY[2],SW_HIDE);}
            }
            yPos+=28;
            // Pressures + Prefix row
            CreateWindowA("STATIC","Pressures:",WS_CHILD|WS_VISIBLE,10,yPos+2,60,20,hwnd,(HMENU)421,hi,NULL);
            g_hPressures=CreateWindowA("EDIT","auto",WS_CHILD|WS_VISIBLE|WS_BORDER|ES_AUTOHSCROLL,72,yPos,180,22,hwnd,NULL,hi,NULL);
            CreateWindowA("STATIC","Prefix:",WS_CHILD|WS_VISIBLE,267,yPos+2,38,20,hwnd,(HMENU)422,hi,NULL);
            g_hPrefix=CreateWindowA("EDIT","IAST_result",WS_CHILD|WS_VISIBLE|WS_BORDER|ES_AUTOHSCROLL,307,yPos,200,22,hwnd,NULL,hi,NULL);
            yPos+=28;
            // Ternary toggle + Run/Exit
            CreateWindowA("BUTTON","Ternary (3 gases)",WS_CHILD|WS_VISIBLE|BS_AUTOCHECKBOX,10,yPos,155,22,hwnd,(HMENU)200,hi,NULL);
            g_hRunBtn=CreateWindowA("BUTTON","Run",WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON,550,yPos-2,55,24,hwnd,(HMENU)201,hi,NULL);
            CreateWindowA("BUTTON","Exit",WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON,610,yPos-2,55,24,hwnd,(HMENU)202,hi,NULL);
            yPos+=28;
            // Chart type dropdown + Save buttons
            CreateWindowA("STATIC","Chart:",WS_CHILD|WS_VISIBLE,10,yPos+2,40,20,hwnd,(HMENU)430,hi,NULL);
            g_hChartCmb=CreateWindowA("COMBOBOX","",WS_CHILD|WS_VISIBLE|CBS_DROPDOWNLIST,52,yPos,180,100,hwnd,(HMENU)300,hi,NULL);
            SendMessageA(g_hChartCmb,CB_ADDSTRING,0,(LPARAM)"Pure Isotherms");
            SendMessageA(g_hChartCmb,CB_ADDSTRING,0,(LPARAM)"Mixture Uptake");
            SendMessageA(g_hChartCmb,CB_ADDSTRING,0,(LPARAM)"Selectivity");
            SendMessageA(g_hChartCmb,CB_ADDSTRING,0,(LPARAM)"Separation Potential");
            SendMessage(g_hChartCmb,CB_SETCURSEL,0,0);
            // Save Chart / Save Log buttons
            CreateWindowA("BUTTON","Save Chart",WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON,460,yPos,80,24,hwnd,(HMENU)500,hi,NULL);
            CreateWindowA("BUTTON","Save Log",WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON,545,yPos,80,24,hwnd,(HMENU)501,hi,NULL);
            yPos+=28;
            // Chart area
            g_hChart=CreateWindowExW(0,L"IastChart",L"",WS_CHILD|WS_VISIBLE|WS_BORDER,10,yPos,675,320,hwnd,NULL,hi,NULL);
            // Log area
            g_hLog=CreateWindowA("EDIT","",WS_CHILD|WS_VISIBLE|WS_BORDER|ES_MULTILINE|ES_READONLY|WS_VSCROLL|ES_AUTOVSCROLL,10,yPos+326,675,140,hwnd,NULL,hi,NULL);
            SendMessage(g_hLog,EM_SETLIMITTEXT,0,0);
            HFONT hFont=CreateFontA(16,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,DEFAULT_QUALITY,DEFAULT_PITCH,"Segoe UI");
            SendMessage(g_hLog,WM_SETFONT,(WPARAM)hFont,TRUE);
            return 0;
        }
        case WM_SIZE:{
            int w=(int)LOWORD(lp),ht=(int)HIWORD(lp);
            if(w<650)w=650;if(ht<480)ht=480;
            // Stretch params
            for(int i=0;i<3;++i)SetWindowPos(g_hParams[i],NULL,0,0,max(130,w-520),22,SWP_NOMOVE|SWP_NOZORDER);
            // Mixture y row (G1/G2/G3 y inputs) - keep at original position
            // int myr=86; // no longer needed for Pressures
            // Pressures + Prefix row (independent row below y inputs)
            int myrPress=115; // y position for Pressures/Prefix row
            int presW=max(100,min(180,w-580)); // cap Pressures width to leave room for Prefix
            SetWindowPos(g_hPressures,NULL,72,myrPress,presW,22,SWP_NOZORDER);
            SetWindowPos(GetDlgItem(hwnd,421),NULL,10,myrPress+2,60,20,SWP_NOZORDER);
            SetWindowPos(GetDlgItem(hwnd,422),NULL,72+presW+15,myrPress+2,38,20,SWP_NOZORDER);
            int prefX=72+presW+55;
            SetWindowPos(g_hPrefix,NULL,prefX,myrPress,max(80,w-prefX-12),22,SWP_NOZORDER);
            // Buttons right-align (Ternary row)
            SetWindowPos(GetDlgItem(hwnd,200),NULL,10,myrPress+28+2,120,20,SWP_NOZORDER);
            SetWindowPos(g_hRunBtn,NULL,w-145,myrPress+28,0,0,SWP_NOSIZE|SWP_NOZORDER);
            SetWindowPos(GetDlgItem(hwnd,202),NULL,w-80,myrPress+28,0,0,SWP_NOSIZE|SWP_NOZORDER);
            // Chart row (above chart area)
            int tabsY=myrPress+56;
            SetWindowPos(g_hChartCmb,NULL,52,tabsY,max(100,min(180,w-260)),24,SWP_NOZORDER);
            // Save buttons right-aligned
            SetWindowPos(GetDlgItem(hwnd,500),NULL,w-170,tabsY,80,24,SWP_NOSIZE|SWP_NOZORDER);
            SetWindowPos(GetDlgItem(hwnd,501),NULL,w-85,tabsY,80,24,SWP_NOSIZE|SWP_NOZORDER);
            // Chart + Log
            int chartTop=tabsY+28,logH=150,margin=10;
            SetWindowPos(g_hChart,NULL,margin,chartTop,w-margin*2-14,ht-chartTop-logH-margin*2,SWP_NOZORDER);
            SetWindowPos(g_hLog,NULL,margin,ht-logH-margin,w-margin*2-14,logH,SWP_NOZORDER);
            return 0;
        }
        case WM_GETMINMAXINFO:{
            MINMAXINFO*mmi=(MINMAXINFO*)lp;mmi->ptMinTrackSize.x=650;mmi->ptMinTrackSize.y=480;return 0;
        }
        case WM_COMMAND:{
            int id=LOWORD(wp);
            if(id>=100&&id<=102){
                wchar_t bf[MAX_PATH]={0};OPENFILENAMEW ofn={sizeof(ofn)};ofn.hwndOwner=hwnd;
                ofn.lpstrFilter=L"CSV\0*.csv\0All\0*.*\0";ofn.lpstrFile=bf;ofn.nMaxFile=MAX_PATH;
                ofn.Flags=OFN_FILEMUSTEXIST|OFN_HIDEREADONLY;
                if(GetOpenFileNameW(&ofn))SetWindowTextW(g_hCsvPaths[id-100],bf);
            }else if(id==300&&HIWORD(wp)==CBN_SELCHANGE){
                g_activeChart=max(0,min(3,(int)SendMessage(g_hChartCmb,CB_GETCURSEL,0,0)));
                RefreshChart();
            }else if(id==500){SaveChartImage(hwnd);}
            else if(id==501){SaveLog(hwnd);}
            else if(id==200){
                bool tn=SendMessage((HWND)lp,BM_GETCHECK,0,0)==BST_CHECKED;g_numGases=tn?3:2;
                int sw=tn?SW_SHOW:SW_HIDE;
                ShowWindow(g_hCsvPaths[2],sw);
                ShowWindow(g_hModelCmb[2],sw);ShowWindow(g_hParams[2],sw);
                ShowWindow(g_hY[2],sw);ShowWindow(GetDlgItem(hwnd,432),sw);
            }else if(id==201){
                if(g_calculating){MessageBoxA(hwnd,"Calculation in progress...","Info",MB_OK);return 0;}
                SetWindowTextA(g_hLog,"");g_logText.clear();
                for(int i=0;i<4;++i){g_chartSeriesCount[i]=0;g_chartX[i].clear();for(int j=0;j<3;++j)g_chartY[i][j].clear();}
                RefreshChart();CreateThread(NULL,0,CalcThread,NULL,0,NULL);
            }else if(id==202){DestroyWindow(hwnd);}
            return 0;
        }
        case WM_USER+1:{
            std::lock_guard<std::mutex>lk(g_resultMutex);
            SetWindowTextA(g_hLog,g_logText.c_str());
            RefreshChart();EnableWindow(g_hRunBtn,TRUE);return 0;
        }
        case WM_DESTROY:PostQuitMessage(0);return 0;
    }
    return DefWindowProc(hwnd,msg,wp,lp);
}

// ─── WinMain ────────────────────────────────────────────────
std::string WideToUtf8(const wchar_t* value){
    int size=WideCharToMultiByte(CP_UTF8,0,value,-1,NULL,0,NULL,NULL);
    if(size<=0)return std::string();
    std::string result(size,'\0');
    WideCharToMultiByte(CP_UTF8,0,value,-1,result.data(),size,NULL,NULL);
    result.pop_back();
    return result;
}

int TryRunBatchMode(){
    int argc=0;LPWSTR* wideArgv=CommandLineToArgvW(GetCommandLineW(),&argc);
    if(!wideArgv)return -1;
    bool batch=false;for(int i=1;i<argc;++i)if(wcscmp(wideArgv[i],L"--batch")==0)batch=true;
    if(!batch){LocalFree(wideArgv);return -1;}
    std::vector<std::string> args;args.reserve(argc);
    for(int i=0;i<argc;++i)if(wcscmp(wideArgv[i],L"--batch")!=0)args.push_back(WideToUtf8(wideArgv[i]));
    LocalFree(wideArgv);
    std::vector<char*> argv;argv.reserve(args.size());
    for(auto&arg:args)argv.push_back(arg.data());
    return run_iast_cli((int)argv.size(),argv.data());
}

int WINAPI WinMain(HINSTANCE hi,HINSTANCE,LPSTR,int nCmdShow){
    int batchResult=TryRunBatchMode();if(batchResult>=0)return batchResult;
    g_hInst=hi;InitGDIPlus();
    WNDCLASSEXW wc={sizeof(wc)};wc.lpfnWndProc=MainWndProc;wc.hInstance=hi;
    wc.hCursor=LoadCursor(NULL,IDC_ARROW);wc.hbrBackground=(HBRUSH)(COLOR_BTNFACE+1);
    wc.lpszClassName=L"IastCalcWindow";RegisterClassExW(&wc);
    HWND hwnd=CreateWindowExW(0,L"IastCalcWindow",L"IAST Calculator (C++)",
        WS_OVERLAPPEDWINDOW,CW_USEDEFAULT,CW_USEDEFAULT,850,710,NULL,NULL,hi,NULL);
    ShowWindow(hwnd,nCmdShow);UpdateWindow(hwnd);
    MSG msg;while(GetMessage(&msg,NULL,0,0)){TranslateMessage(&msg);DispatchMessage(&msg);}
    ShutdownGDIPlus();return 0;
}
