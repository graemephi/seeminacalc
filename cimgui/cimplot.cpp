//This file is automatically generated by generator.lua from https://github.com/cimgui/cimgui
//based on implot.h file version 0.3 WIP from Dear ImGui https://github.com/ocornut/imgui

#include "./implot/implot.h"
#include "cimplot.h"



CIMGUI_API ImPlotPoint* ImPlotPoint_ImPlotPointNil(void)
{
    return IM_NEW(ImPlotPoint)();
}
CIMGUI_API void ImPlotPoint_destroy(ImPlotPoint* self)
{
    IM_DELETE(self);
}
CIMGUI_API ImPlotPoint* ImPlotPoint_ImPlotPointdouble(double _x,double _y)
{
    return IM_NEW(ImPlotPoint)(_x,_y);
}
CIMGUI_API ImPlotRange* ImPlotRange_ImPlotRange(void)
{
    return IM_NEW(ImPlotRange)();
}
CIMGUI_API void ImPlotRange_destroy(ImPlotRange* self)
{
    IM_DELETE(self);
}
CIMGUI_API bool ImPlotRange_Contains(ImPlotRange* self,double value)
{
    return self->Contains(value);
}
CIMGUI_API double ImPlotRange_Size(ImPlotRange* self)
{
    return self->Size();
}
CIMGUI_API ImPlotLimits* ImPlotLimits_ImPlotLimits(void)
{
    return IM_NEW(ImPlotLimits)();
}
CIMGUI_API void ImPlotLimits_destroy(ImPlotLimits* self)
{
    IM_DELETE(self);
}
CIMGUI_API bool ImPlotLimits_ContainsPlotPoInt(ImPlotLimits* self,const ImPlotPoint p)
{
    return self->Contains(p);
}
CIMGUI_API bool ImPlotLimits_Containsdouble(ImPlotLimits* self,double x,double y)
{
    return self->Contains(x,y);
}
CIMGUI_API ImPlotStyle* ImPlotStyle_ImPlotStyle(void)
{
    return IM_NEW(ImPlotStyle)();
}
CIMGUI_API void ImPlotStyle_destroy(ImPlotStyle* self)
{
    IM_DELETE(self);
}
CIMGUI_API bool ipBeginPlot(const char* title_id,const char* x_label,const char* y_label,const ImVec2 size,ImPlotFlags flags,ImPlotAxisFlags x_flags,ImPlotAxisFlags y_flags,ImPlotAxisFlags y2_flags,ImPlotAxisFlags y3_flags)
{
    return ImPlot::BeginPlot(title_id,x_label,y_label,size,flags,x_flags,y_flags,y2_flags,y3_flags);
}
CIMGUI_API void ipEndPlot()
{
    return ImPlot::EndPlot();
}
CIMGUI_API void ipPlotLineFloatPtrInt(const char* label_id,const float* values,int count,int offset,int stride)
{
    return ImPlot::PlotLine(label_id,values,count,offset,stride);
}
CIMGUI_API void ipPlotLinedoublePtrInt(const char* label_id,const double* values,int count,int offset,int stride)
{
    return ImPlot::PlotLine(label_id,values,count,offset,stride);
}
CIMGUI_API void ipPlotLineFloatPtrFloatPtr(const char* label_id,const float* xs,const float* ys,int count,int offset,int stride)
{
    return ImPlot::PlotLine(label_id,xs,ys,count,offset,stride);
}
CIMGUI_API void ipPlotLinedoublePtrdoublePtr(const char* label_id,const double* xs,const double* ys,int count,int offset,int stride)
{
    return ImPlot::PlotLine(label_id,xs,ys,count,offset,stride);
}
CIMGUI_API void ipPlotLineVec2Ptr(const char* label_id,const ImVec2* data,int count,int offset)
{
    return ImPlot::PlotLine(label_id,data,count,offset);
}
CIMGUI_API void ipPlotLinePlotPoIntPtr(const char* label_id,const ImPlotPoint* data,int count,int offset)
{
    return ImPlot::PlotLine(label_id,data,count,offset);
}
CIMGUI_API void ipPlotLineFnPlotPoIntPtr(const char* label_id,ImPlotPoint(*getter)(void* data,int idx),void* data,int count,int offset)
{
    return ImPlot::PlotLine(label_id,getter,data,count,offset);
}
CIMGUI_API void ipPlotScatterFloatPtrInt(const char* label_id,const float* values,int count,int offset,int stride)
{
    return ImPlot::PlotScatter(label_id,values,count,offset,stride);
}
CIMGUI_API void ipPlotScatterdoublePtrInt(const char* label_id,const double* values,int count,int offset,int stride)
{
    return ImPlot::PlotScatter(label_id,values,count,offset,stride);
}
CIMGUI_API void ipPlotScatterFloatPtrFloatPtr(const char* label_id,const float* xs,const float* ys,int count,int offset,int stride)
{
    return ImPlot::PlotScatter(label_id,xs,ys,count,offset,stride);
}
CIMGUI_API void ipPlotScatterdoublePtrdoublePtr(const char* label_id,const double* xs,const double* ys,int count,int offset,int stride)
{
    return ImPlot::PlotScatter(label_id,xs,ys,count,offset,stride);
}
CIMGUI_API void ipPlotScatterVec2Ptr(const char* label_id,const ImVec2* data,int count,int offset)
{
    return ImPlot::PlotScatter(label_id,data,count,offset);
}
CIMGUI_API void ipPlotScatterPlotPoIntPtr(const char* label_id,const ImPlotPoint* data,int count,int offset)
{
    return ImPlot::PlotScatter(label_id,data,count,offset);
}
CIMGUI_API void ipPlotScatterFnPlotPoIntPtr(const char* label_id,ImPlotPoint(*getter)(void* data,int idx),void* data,int count,int offset)
{
    return ImPlot::PlotScatter(label_id,getter,data,count,offset);
}
CIMGUI_API void ipPlotBarsFloatPtrIntFloat(const char* label_id,const float* values,int count,float width,float shift,int offset,int stride)
{
    return ImPlot::PlotBars(label_id,values,count,width,shift,offset,stride);
}
CIMGUI_API void ipPlotBarsdoublePtrIntdouble(const char* label_id,const double* values,int count,double width,double shift,int offset,int stride)
{
    return ImPlot::PlotBars(label_id,values,count,width,shift,offset,stride);
}
CIMGUI_API void ipPlotBarsFloatPtrFloatPtr(const char* label_id,const float* xs,const float* ys,int count,float width,int offset,int stride)
{
    return ImPlot::PlotBars(label_id,xs,ys,count,width,offset,stride);
}
CIMGUI_API void ipPlotBarsdoublePtrdoublePtr(const char* label_id,const double* xs,const double* ys,int count,double width,int offset,int stride)
{
    return ImPlot::PlotBars(label_id,xs,ys,count,width,offset,stride);
}
CIMGUI_API void ipPlotBarsFnPlotPoIntPtr(const char* label_id,ImPlotPoint(*getter)(void* data,int idx),void* data,int count,double width,int offset)
{
    return ImPlot::PlotBars(label_id,getter,data,count,width,offset);
}
CIMGUI_API void ipPlotBarsHFloatPtrIntFloat(const char* label_id,const float* values,int count,float height,float shift,int offset,int stride)
{
    return ImPlot::PlotBarsH(label_id,values,count,height,shift,offset,stride);
}
CIMGUI_API void ipPlotBarsHdoublePtrIntdouble(const char* label_id,const double* values,int count,double height,double shift,int offset,int stride)
{
    return ImPlot::PlotBarsH(label_id,values,count,height,shift,offset,stride);
}
CIMGUI_API void ipPlotBarsHFloatPtrFloatPtr(const char* label_id,const float* xs,const float* ys,int count,float height,int offset,int stride)
{
    return ImPlot::PlotBarsH(label_id,xs,ys,count,height,offset,stride);
}
CIMGUI_API void ipPlotBarsHdoublePtrdoublePtr(const char* label_id,const double* xs,const double* ys,int count,double height,int offset,int stride)
{
    return ImPlot::PlotBarsH(label_id,xs,ys,count,height,offset,stride);
}
CIMGUI_API void ipPlotBarsHFnPlotPoIntPtr(const char* label_id,ImPlotPoint(*getter)(void* data,int idx),void* data,int count,double height,int offset)
{
    return ImPlot::PlotBarsH(label_id,getter,data,count,height,offset);
}
CIMGUI_API void ipPlotErrorBarsFloatPtrFloatPtrFloatPtrInt(const char* label_id,const float* xs,const float* ys,const float* err,int count,int offset,int stride)
{
    return ImPlot::PlotErrorBars(label_id,xs,ys,err,count,offset,stride);
}
CIMGUI_API void ipPlotErrorBarsdoublePtrdoublePtrdoublePtrInt(const char* label_id,const double* xs,const double* ys,const double* err,int count,int offset,int stride)
{
    return ImPlot::PlotErrorBars(label_id,xs,ys,err,count,offset,stride);
}
CIMGUI_API void ipPlotErrorBarsFloatPtrFloatPtrFloatPtrFloatPtr(const char* label_id,const float* xs,const float* ys,const float* neg,const float* pos,int count,int offset,int stride)
{
    return ImPlot::PlotErrorBars(label_id,xs,ys,neg,pos,count,offset,stride);
}
CIMGUI_API void ipPlotErrorBarsdoublePtrdoublePtrdoublePtrdoublePtr(const char* label_id,const double* xs,const double* ys,const double* neg,const double* pos,int count,int offset,int stride)
{
    return ImPlot::PlotErrorBars(label_id,xs,ys,neg,pos,count,offset,stride);
}
CIMGUI_API void ipPlotErrorBarsHFloatPtrFloatPtrFloatPtrInt(const char* label_id,const float* xs,const float* ys,const float* err,int count,int offset,int stride)
{
    return ImPlot::PlotErrorBarsH(label_id,xs,ys,err,count,offset,stride);
}
CIMGUI_API void ipPlotErrorBarsHdoublePtrdoublePtrdoublePtrInt(const char* label_id,const double* xs,const double* ys,const double* err,int count,int offset,int stride)
{
    return ImPlot::PlotErrorBarsH(label_id,xs,ys,err,count,offset,stride);
}
CIMGUI_API void ipPlotErrorBarsHFloatPtrFloatPtrFloatPtrFloatPtr(const char* label_id,const float* xs,const float* ys,const float* neg,const float* pos,int count,int offset,int stride)
{
    return ImPlot::PlotErrorBarsH(label_id,xs,ys,neg,pos,count,offset,stride);
}
CIMGUI_API void ipPlotErrorBarsHdoublePtrdoublePtrdoublePtrdoublePtr(const char* label_id,const double* xs,const double* ys,const double* neg,const double* pos,int count,int offset,int stride)
{
    return ImPlot::PlotErrorBarsH(label_id,xs,ys,neg,pos,count,offset,stride);
}
CIMGUI_API void ipPlotPieChartFloatPtr(const char** label_ids,const float* values,int count,float x,float y,float radius,bool normalize,const char* label_fmt,float angle0)
{
    return ImPlot::PlotPieChart(label_ids,values,count,x,y,radius,normalize,label_fmt,angle0);
}
CIMGUI_API void ipPlotPieChartdoublePtr(const char** label_ids,const double* values,int count,double x,double y,double radius,bool normalize,const char* label_fmt,double angle0)
{
    return ImPlot::PlotPieChart(label_ids,values,count,x,y,radius,normalize,label_fmt,angle0);
}
CIMGUI_API void ipPlotHeatmapFloatPtr(const char* label_id,const float* values,int rows,int cols,float scale_min,float scale_max,const char* label_fmt,const ImPlotPoint bounds_min,const ImPlotPoint bounds_max)
{
    return ImPlot::PlotHeatmap(label_id,values,rows,cols,scale_min,scale_max,label_fmt,bounds_min,bounds_max);
}
CIMGUI_API void ipPlotHeatmapdoublePtr(const char* label_id,const double* values,int rows,int cols,double scale_min,double scale_max,const char* label_fmt,const ImPlotPoint bounds_min,const ImPlotPoint bounds_max)
{
    return ImPlot::PlotHeatmap(label_id,values,rows,cols,scale_min,scale_max,label_fmt,bounds_min,bounds_max);
}
CIMGUI_API void ipPlotDigitalFloatPtr(const char* label_id,const float* xs,const float* ys,int count,int offset,int stride)
{
    return ImPlot::PlotDigital(label_id,xs,ys,count,offset,stride);
}
CIMGUI_API void ipPlotDigitaldoublePtr(const char* label_id,const double* xs,const double* ys,int count,int offset,int stride)
{
    return ImPlot::PlotDigital(label_id,xs,ys,count,offset,stride);
}
CIMGUI_API void ipPlotDigitalFnPlotPoIntPtr(const char* label_id,ImPlotPoint(*getter)(void* data,int idx),void* data,int count,int offset)
{
    return ImPlot::PlotDigital(label_id,getter,data,count,offset);
}
CIMGUI_API void ipPlotTextFloat(const char* text,float x,float y,bool vertical,const ImVec2 pixel_offset)
{
    return ImPlot::PlotText(text,x,y,vertical,pixel_offset);
}
CIMGUI_API void ipPlotTextdouble(const char* text,double x,double y,bool vertical,const ImVec2 pixel_offset)
{
    return ImPlot::PlotText(text,x,y,vertical,pixel_offset);
}
CIMGUI_API bool ipIsPlotHovered()
{
    return ImPlot::IsPlotHovered();
}
CIMGUI_API void ipGetPlotMousePos(ImPlotPoint *pOut,int y_axis)
{
    *pOut = ImPlot::GetPlotMousePos(y_axis);
}
CIMGUI_API void ipGetPlotLimits(ImPlotLimits *pOut,int y_axis)
{
    *pOut = ImPlot::GetPlotLimits(y_axis);
}
CIMGUI_API bool ipIsPlotQueried()
{
    return ImPlot::IsPlotQueried();
}
CIMGUI_API void ipGetPlotQuery(ImPlotLimits *pOut,int y_axis)
{
    *pOut = ImPlot::GetPlotQuery(y_axis);
}
CIMGUI_API ImPlotStyle* ipGetStyle()
{
    return &ImPlot::GetStyle();
}
CIMGUI_API void ipPushStyleColorU32(ImPlotCol idx,ImU32 col)
{
    return ImPlot::PushStyleColor(idx,col);
}
CIMGUI_API void ipPushStyleColorVec4(ImPlotCol idx,const ImVec4 col)
{
    return ImPlot::PushStyleColor(idx,col);
}
CIMGUI_API void ipPopStyleColor(int count)
{
    return ImPlot::PopStyleColor(count);
}
CIMGUI_API void ipPushStyleVarFloat(ImPlotStyleVar idx,float val)
{
    return ImPlot::PushStyleVar(idx,val);
}
CIMGUI_API void ipPushStyleVarInt(ImPlotStyleVar idx,int val)
{
    return ImPlot::PushStyleVar(idx,val);
}
CIMGUI_API void ipPopStyleVar(int count)
{
    return ImPlot::PopStyleVar(count);
}
CIMGUI_API void ipSetColormapPlotColormap(ImPlotColormap colormap,int samples)
{
    return ImPlot::SetColormap(colormap,samples);
}
CIMGUI_API void ipSetColormapVec4Ptr(const ImVec4* colors,int num_colors)
{
    return ImPlot::SetColormap(colors,num_colors);
}
CIMGUI_API int ipGetColormapSize()
{
    return ImPlot::GetColormapSize();
}
CIMGUI_API void ipGetColormapColor(ImVec4 *pOut,int index)
{
    *pOut = ImPlot::GetColormapColor(index);
}
CIMGUI_API void ipLerpColormap(ImVec4 *pOut,float t)
{
    *pOut = ImPlot::LerpColormap(t);
}
CIMGUI_API void ipSetNextPlotLimits(double x_min,double x_max,double y_min,double y_max,ImGuiCond cond)
{
    return ImPlot::SetNextPlotLimits(x_min,x_max,y_min,y_max,cond);
}
CIMGUI_API void ipSetNextPlotLimitsX(double x_min,double x_max,ImGuiCond cond)
{
    return ImPlot::SetNextPlotLimitsX(x_min,x_max,cond);
}
CIMGUI_API void ipSetNextPlotLimitsY(double y_min,double y_max,ImGuiCond cond,int y_axis)
{
    return ImPlot::SetNextPlotLimitsY(y_min,y_max,cond,y_axis);
}
CIMGUI_API void ipSetNextPlotTicksXdoublePtr(const double* values,int n_ticks,const char** labels,bool show_default)
{
    return ImPlot::SetNextPlotTicksX(values,n_ticks,labels,show_default);
}
CIMGUI_API void ipSetNextPlotTicksXdouble(double x_min,double x_max,int n_ticks,const char** labels,bool show_default)
{
    return ImPlot::SetNextPlotTicksX(x_min,x_max,n_ticks,labels,show_default);
}
CIMGUI_API void ipSetNextPlotTicksYdoublePtr(const double* values,int n_ticks,const char** labels,bool show_default,int y_axis)
{
    return ImPlot::SetNextPlotTicksY(values,n_ticks,labels,show_default,y_axis);
}
CIMGUI_API void ipSetNextPlotTicksYdouble(double y_min,double y_max,int n_ticks,const char** labels,bool show_default,int y_axis)
{
    return ImPlot::SetNextPlotTicksY(y_min,y_max,n_ticks,labels,show_default,y_axis);
}
CIMGUI_API void ipSetPlotYAxis(int y_axis)
{
    return ImPlot::SetPlotYAxis(y_axis);
}
CIMGUI_API void ipGetPlotPos(ImVec2 *pOut)
{
    *pOut = ImPlot::GetPlotPos();
}
CIMGUI_API void ipGetPlotSize(ImVec2 *pOut)
{
    *pOut = ImPlot::GetPlotSize();
}
CIMGUI_API void ipPixelsToPlot(ImPlotPoint *pOut,const ImVec2 pix,int y_axis)
{
    *pOut = ImPlot::PixelsToPlot(pix,y_axis);
}
CIMGUI_API void ipPlotToPixels(ImVec2 *pOut,const ImPlotPoint plt,int y_axis)
{
    *pOut = ImPlot::PlotToPixels(plt,y_axis);
}
CIMGUI_API void ipShowColormapScale(double scale_min,double scale_max,float height)
{
    return ImPlot::ShowColormapScale(scale_min,scale_max,height);
}
CIMGUI_API void ipPushPlotClipRect()
{
    return ImPlot::PushPlotClipRect();
}
CIMGUI_API void ipPopPlotClipRect()
{
    return ImPlot::PopPlotClipRect();
}
CIMGUI_API void ipShowDemoWindow(bool* p_open)
{
    return ImPlot::ShowDemoWindow(p_open);
}
