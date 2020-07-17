#include "cimgui/imgui/imgui.cpp"
#include "cimgui/imgui/imgui_demo.cpp"
#include "cimgui/imgui/imgui_draw.cpp"
#include "cimgui/imgui/imgui_widgets.cpp"

#include "cimgui/implot/implot.cpp"
#include "cimgui/implot/implot_demo.cpp"

#define CIMGUI_NO_EXPORT
#include "cimgui/cimgui.cpp"
#include "cimgui/cimplot.cpp"

#include "cminacalc.cpp"

// ImPlot's zoom sucks??? So just hack away
namespace ImPlot {

// Copy n pasted from implot.cpp
extern "C" bool BeginPlotCppCpp(const char* title, const char* x_label, const char* y_label, const ImVec2* sizep, ImPlotFlags flags, ImPlotAxisFlags x_flags, ImPlotAxisFlags y_flags, ImPlotAxisFlags y2_flags, ImPlotAxisFlags y3_flags) {

    IM_ASSERT_USER_ERROR(gp.CurrentPlot == NULL, "Mismatched BeginPlot()/EndPlot()!");
    const ImVec2& size = *sizep;

    // FRONT MATTER  -----------------------------------------------------------

    ImGuiContext &G      = *GImGui;
    ImGuiWindow * Window = G.CurrentWindow;
    if (Window->SkipItems) {
        gp.Reset();
        return false;
    }

    const ImGuiID     ID       = Window->GetID(title);
    const ImGuiStyle &Style    = G.Style;
    const ImGuiIO &   IO       = ImGui::GetIO();

    bool just_created = gp.Plots.GetByKey(ID) == NULL;
    gp.CurrentPlot = gp.Plots.GetOrAddByKey(ID);
    ImPlotState &plot = *gp.CurrentPlot;

    plot.CurrentYAxis = 0;

    if (just_created) {
        plot.Flags          = flags;
        plot.XAxis.Flags    = x_flags;
        plot.YAxis[0].Flags = y_flags;
        plot.YAxis[1].Flags = y2_flags;
        plot.YAxis[2].Flags = y3_flags;
    }
    else {
        // TODO: Check which individual flags changed, and only reset those!
        // There's probably an easy bit mask trick I'm not aware of.
        if (flags != plot.PreviousFlags)
            plot.Flags = flags;
        if (y_flags != plot.YAxis[0].PreviousFlags)
            plot.YAxis[0].PreviousFlags = y_flags;
        if (y2_flags != plot.YAxis[1].PreviousFlags)
            plot.YAxis[1].PreviousFlags = y2_flags;
        if (y3_flags != plot.YAxis[2].PreviousFlags)
            plot.YAxis[2].PreviousFlags = y3_flags;
    }

    plot.PreviousFlags          = flags;
    plot.XAxis.PreviousFlags    = x_flags;
    plot.YAxis[0].PreviousFlags = y_flags;
    plot.YAxis[1].PreviousFlags = y2_flags;
    plot.YAxis[2].PreviousFlags = y3_flags;

    // capture scroll with a child region
    const float default_w = 400;
    const float default_h = 300;
    if (!HasFlag(plot.Flags, ImPlotFlags_NoChild)) {
        ImGui::BeginChild(title, ImVec2(size.x == 0 ? default_w : size.x, size.y == 0 ? default_h : size.y));
        Window = ImGui::GetCurrentWindow();
        Window->ScrollMax.y = 1.0f;
        gp.ChildWindowMade = true;
    }
    else {
        gp.ChildWindowMade = false;
    }

    ImDrawList &DrawList = *Window->DrawList;

    // NextPlotData -----------------------------------------------------------

    if (gp.NextPlotData.HasXRange) {
        if (just_created || gp.NextPlotData.XRangeCond == ImGuiCond_Always)
        {
            plot.XAxis.Range = gp.NextPlotData.X;
        }
    }

    for (int i = 0; i < MAX_Y_AXES; i++) {
        if (gp.NextPlotData.HasYRange[i]) {
            if (just_created || gp.NextPlotData.YRangeCond[i] == ImGuiCond_Always)
            {
                plot.YAxis[i].Range = gp.NextPlotData.Y[i];
            }
        }
    }

    // AXIS STATES ------------------------------------------------------------
    gp.X    = ImPlotAxisState(plot.XAxis, gp.NextPlotData.HasXRange, gp.NextPlotData.XRangeCond, true, 0);
    gp.Y[0] = ImPlotAxisState(plot.YAxis[0], gp.NextPlotData.HasYRange[0], gp.NextPlotData.YRangeCond[0], true, 0);
    gp.Y[1] = ImPlotAxisState(plot.YAxis[1], gp.NextPlotData.HasYRange[1], gp.NextPlotData.YRangeCond[1],
                                   HasFlag(plot.Flags, ImPlotFlags_YAxis2), gp.Y[0].PresentSoFar);
    gp.Y[2] = ImPlotAxisState(plot.YAxis[2], gp.NextPlotData.HasYRange[2], gp.NextPlotData.YRangeCond[2],
                                   HasFlag(plot.Flags, ImPlotFlags_YAxis3), gp.Y[1].PresentSoFar);

    gp.LockPlot = gp.X.Lock && gp.Y[0].Lock && gp.Y[1].Lock && gp.Y[2].Lock;

    // CONSTRAINTS ------------------------------------------------------------

    plot.XAxis.Range.Min = ConstrainNan(ConstrainInf(plot.XAxis.Range.Min));
    plot.XAxis.Range.Max = ConstrainNan(ConstrainInf(plot.XAxis.Range.Max));
    for (int i = 0; i < MAX_Y_AXES; i++) {
        plot.YAxis[i].Range.Min = ConstrainNan(ConstrainInf(plot.YAxis[i].Range.Min));
        plot.YAxis[i].Range.Max = ConstrainNan(ConstrainInf(plot.YAxis[i].Range.Max));
    }

    if (HasFlag(plot.XAxis.Flags, ImPlotAxisFlags_LogScale))
        plot.XAxis.Range.Min = ConstrainLog(plot.XAxis.Range.Min);
    if (HasFlag(plot.XAxis.Flags, ImPlotAxisFlags_LogScale))
        plot.XAxis.Range.Max = ConstrainLog(plot.XAxis.Range.Max);
    for (int i = 0; i < MAX_Y_AXES; i++) {
        if (HasFlag(plot.YAxis[i].Flags, ImPlotAxisFlags_LogScale))
            plot.YAxis[i].Range.Min = ConstrainLog(plot.YAxis[i].Range.Min);
        if (HasFlag(plot.YAxis[i].Flags, ImPlotAxisFlags_LogScale))
            plot.YAxis[i].Range.Max = ConstrainLog(plot.YAxis[i].Range.Max);
    }

    if (plot.XAxis.Range.Max <= plot.XAxis.Range.Min)
        plot.XAxis.Range.Max = plot.XAxis.Range.Min + DBL_EPSILON;
    for (int i = 0; i < MAX_Y_AXES; i++) {
        if (plot.YAxis[i].Range.Max <= plot.YAxis[i].Range.Min)
            plot.YAxis[i].Range.Max = plot.YAxis[i].Range.Min + DBL_EPSILON;
    }

    // adaptive divisions
    int x_divisions = ImMax(2, (int)IM_ROUND(0.003 * gp.BB_Canvas.GetWidth()));
    int y_divisions[MAX_Y_AXES];
    for (int i = 0; i < MAX_Y_AXES; i++) {
        y_divisions[i] = ImMax(2, (int)IM_ROUND(0.003 * gp.BB_Canvas.GetHeight()));
    }

    // COLORS -----------------------------------------------------------------

    gp.Col_Frame  = gp.Style.Colors[ImPlotCol_FrameBg].w     == -1 ? ImGui::GetColorU32(ImGuiCol_FrameBg)    : ImGui::GetColorU32(gp.Style.Colors[ImPlotCol_FrameBg]);
    gp.Col_Bg     = gp.Style.Colors[ImPlotCol_PlotBg].w      == -1 ? ImGui::GetColorU32(ImGuiCol_WindowBg)   : ImGui::GetColorU32(gp.Style.Colors[ImPlotCol_PlotBg]);
    gp.Col_Border = gp.Style.Colors[ImPlotCol_PlotBorder].w  == -1 ? ImGui::GetColorU32(ImGuiCol_Text, 0.5f) : ImGui::GetColorU32(gp.Style.Colors[ImPlotCol_PlotBorder]);

    UpdateAxisColor(ImPlotCol_XAxis, &gp.Col_X);
    UpdateAxisColor(ImPlotCol_YAxis, &gp.Col_Y[0]);
    UpdateAxisColor(ImPlotCol_YAxis2, &gp.Col_Y[1]);
    UpdateAxisColor(ImPlotCol_YAxis3, &gp.Col_Y[2]);

    gp.Col_Txt    = ImGui::GetColorU32(ImGuiCol_Text);
    gp.Col_TxtDis = ImGui::GetColorU32(ImGuiCol_TextDisabled);
    gp.Col_SlctBg = ImGui::GetColorU32(gp.Style.Colors[ImPlotCol_Selection] * ImVec4(1,1,1,0.25f));
    gp.Col_SlctBd = ImGui::GetColorU32(gp.Style.Colors[ImPlotCol_Selection]);
    gp.Col_QryBg =  ImGui::GetColorU32(gp.Style.Colors[ImPlotCol_Query] * ImVec4(1,1,1,0.25f));
    gp.Col_QryBd =  ImGui::GetColorU32(gp.Style.Colors[ImPlotCol_Query]);

    // BB AND HOVER -----------------------------------------------------------

    // frame
    const ImVec2 frame_size = ImGui::CalcItemSize(size, default_w, default_h);
    gp.BB_Frame = ImRect(Window->DC.CursorPos, Window->DC.CursorPos + frame_size);
    ImGui::ItemSize(gp.BB_Frame);
    if (!ImGui::ItemAdd(gp.BB_Frame, 0, &gp.BB_Frame)) {
        gp.Reset();
        return false;
    }
    gp.Hov_Frame = ImGui::ItemHoverable(gp.BB_Frame, ID);
    ImGui::RenderFrame(gp.BB_Frame.Min, gp.BB_Frame.Max, gp.Col_Frame, true, Style.FrameRounding);

    // canvas bb
    gp.BB_Canvas = ImRect(gp.BB_Frame.Min + Style.WindowPadding, gp.BB_Frame.Max - Style.WindowPadding);

    gp.RenderX = (HasFlag(plot.XAxis.Flags, ImPlotAxisFlags_GridLines) ||
                    HasFlag(plot.XAxis.Flags, ImPlotAxisFlags_TickMarks) ||
                    HasFlag(plot.XAxis.Flags, ImPlotAxisFlags_TickLabels)) &&  x_divisions > 1;
    for (int i = 0; i < MAX_Y_AXES; i++) {
        gp.RenderY[i] =
                gp.Y[i].Present &&
                (HasFlag(plot.YAxis[i].Flags, ImPlotAxisFlags_GridLines) ||
                 HasFlag(plot.YAxis[i].Flags, ImPlotAxisFlags_TickMarks) ||
                 HasFlag(plot.YAxis[i].Flags, ImPlotAxisFlags_TickLabels)) &&  y_divisions[i] > 1;
    }
    // get ticks
    if (gp.RenderX && gp.NextPlotData.ShowDefaultTicksX)
        AddDefaultTicks(plot.XAxis.Range, x_divisions, 10, HasFlag(plot.XAxis.Flags, ImPlotAxisFlags_LogScale), gp.XTicks);
    for (int i = 0; i < MAX_Y_AXES; i++) {
        if (gp.RenderY[i] && gp.NextPlotData.ShowDefaultTicksY[i]) {
            AddDefaultTicks(plot.YAxis[i].Range, y_divisions[i], 10, HasFlag(plot.YAxis[i].Flags, ImPlotAxisFlags_LogScale), gp.YTicks[i]);
        }
    }

    // label ticks
    if (HasFlag(plot.XAxis.Flags, ImPlotAxisFlags_TickLabels))
        LabelTicks(gp.XTicks, HasFlag(plot.XAxis.Flags, ImPlotAxisFlags_Scientific), gp.XTickLabels);

    float max_label_width[MAX_Y_AXES] = {};
    for (int i = 0; i < MAX_Y_AXES; i++) {
        if (gp.Y[i].Present && HasFlag(plot.YAxis[i].Flags, ImPlotAxisFlags_TickLabels)) {
            LabelTicks(gp.YTicks[i], HasFlag(plot.YAxis[i].Flags, ImPlotAxisFlags_Scientific), gp.YTickLabels[i]);
            max_label_width[i] = MaxTickLabelWidth(gp.YTicks[i]);
        }
    }

    // grid bb
    const ImVec2 title_size = ImGui::CalcTextSize(title, NULL, true);
    const float txt_off     = 5;
    const float txt_height  = ImGui::GetTextLineHeight();
    const float pad_top     = title_size.x > 0.0f ? txt_height + txt_off : 0;
    const float pad_bot     = (HasFlag(plot.XAxis.Flags, ImPlotAxisFlags_TickLabels) ? txt_height + txt_off : 0) + (x_label ? txt_height + txt_off : 0);
    YPadCalculator y_axis_pad(gp.Y, max_label_width, txt_off);
    const float pad_left    = y_axis_pad(0) + (y_label ? txt_height + txt_off : 0);
    const float pad_right   = y_axis_pad(1) + y_axis_pad(2);
    gp.BB_Plot              = ImRect(gp.BB_Canvas.Min + ImVec2(pad_left, pad_top), gp.BB_Canvas.Max - ImVec2(pad_right, pad_bot));
    gp.Hov_Plot             = gp.BB_Plot.Contains(IO.MousePos);

    // axis region bbs
    const ImRect xAxisRegion_bb(gp.BB_Plot.Min + ImVec2(10, 0), ImVec2(gp.BB_Plot.Max.x, gp.BB_Frame.Max.y) - ImVec2(10, 0));
    const bool   hov_x_axis_region = xAxisRegion_bb.Contains(IO.MousePos);

    // The left labels are referenced to the left of the bounding box.
    gp.AxisLabelReference[0] = gp.BB_Plot.Min.x;
    // If Y axis 1 is present, its labels will be referenced to the
    // right of the bounding box.
    gp.AxisLabelReference[1] = gp.BB_Plot.Max.x;
    // The third axis may be either referenced to the right of the
    // bounding box, or 6 pixels further past the end of the 2nd axis.
    gp.AxisLabelReference[2] =
            !gp.Y[1].Present ?
            gp.BB_Plot.Max.x :
            (gp.AxisLabelReference[1] + y_axis_pad(1) + 6);

    ImRect yAxisRegion_bb[MAX_Y_AXES];
    yAxisRegion_bb[0] = ImRect(ImVec2(gp.BB_Frame.Min.x, gp.BB_Plot.Min.y), ImVec2(gp.BB_Plot.Min.x + 6, gp.BB_Plot.Max.y - 10));
    // The auxiliary y axes are off to the right of the BB grid.
    yAxisRegion_bb[1] = ImRect(ImVec2(gp.BB_Plot.Max.x - 6, gp.BB_Plot.Min.y),
                               gp.BB_Plot.Max + ImVec2(y_axis_pad(1), 0));
    yAxisRegion_bb[2] = ImRect(ImVec2(gp.AxisLabelReference[2] - 6, gp.BB_Plot.Min.y),
                               yAxisRegion_bb[1].Max + ImVec2(y_axis_pad(2), 0));

    ImRect centralRegion(ImVec2(gp.BB_Plot.Min.x + 6, gp.BB_Plot.Min.y),
                         ImVec2(gp.BB_Plot.Max.x - 6, gp.BB_Plot.Max.y));

    const bool hov_y_axis_region[MAX_Y_AXES] = {
        gp.Y[0].Present && (yAxisRegion_bb[0].Contains(IO.MousePos) || centralRegion.Contains(IO.MousePos)),
        gp.Y[1].Present && (yAxisRegion_bb[1].Contains(IO.MousePos) || centralRegion.Contains(IO.MousePos)),
        gp.Y[2].Present && (yAxisRegion_bb[2].Contains(IO.MousePos) || centralRegion.Contains(IO.MousePos)),
    };
    const bool any_hov_y_axis_region = hov_y_axis_region[0] || hov_y_axis_region[1] || hov_y_axis_region[2];

    // legend hovered from last frame
    const bool hov_legend = HasFlag(plot.Flags, ImPlotFlags_Legend) ? gp.Hov_Frame && plot.BB_Legend.Contains(IO.MousePos) : false;

    bool hov_query = false;
    if (gp.Hov_Frame && gp.Hov_Plot && plot.Queried && !plot.Querying) {
        ImRect bb_query = plot.QueryRect;

        bb_query.Min += gp.BB_Plot.Min;
        bb_query.Max += gp.BB_Plot.Min;

        hov_query = bb_query.Contains(IO.MousePos);
    }

    // QUERY DRAG -------------------------------------------------------------
    if (plot.DraggingQuery && (IO.MouseReleased[gp.InputMap.PanButton] || !IO.MouseDown[gp.InputMap.PanButton])) {
        plot.DraggingQuery = false;
    }
    if (plot.DraggingQuery) {
        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);
        plot.QueryRect.Min += IO.MouseDelta;
        plot.QueryRect.Max += IO.MouseDelta;
    }
    if (gp.Hov_Frame && gp.Hov_Plot && hov_query && !plot.DraggingQuery && !plot.Selecting && !hov_legend) {
        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);
        const bool any_y_dragging = plot.YAxis[0].Dragging || plot.YAxis[1].Dragging || plot.YAxis[2].Dragging;
        if (IO.MouseDown[gp.InputMap.PanButton] && !plot.XAxis.Dragging && !any_y_dragging) {
            plot.DraggingQuery = true;
        }
    }

    // DRAG INPUT -------------------------------------------------------------

    // end drags
    if (plot.XAxis.Dragging && (IO.MouseReleased[gp.InputMap.PanButton] || !IO.MouseDown[gp.InputMap.PanButton])) {
        plot.XAxis.Dragging             = false;
        G.IO.MouseDragMaxDistanceSqr[0] = 0;
    }
    for (int i = 0; i < MAX_Y_AXES; i++) {
        if (plot.YAxis[i].Dragging && (IO.MouseReleased[gp.InputMap.PanButton] || !IO.MouseDown[gp.InputMap.PanButton])) {
            plot.YAxis[i].Dragging             = false;
            G.IO.MouseDragMaxDistanceSqr[0] = 0;
        }
    }
    const bool any_y_dragging = plot.YAxis[0].Dragging || plot.YAxis[1].Dragging || plot.YAxis[2].Dragging;
    bool drag_in_progress = plot.XAxis.Dragging || any_y_dragging;
    // do drag
    if (drag_in_progress) {
        UpdateTransformCache();
        if (!gp.X.Lock && plot.XAxis.Dragging) {
            ImPlotPoint plot_tl = PixelsToPlot(gp.BB_Plot.Min - IO.MouseDelta, 0);
            ImPlotPoint plot_br = PixelsToPlot(gp.BB_Plot.Max - IO.MouseDelta, 0);
            if (!gp.X.LockMin)
                plot.XAxis.Range.Min = gp.X.Invert ? plot_br.x : plot_tl.x;
            if (!gp.X.LockMax)
                plot.XAxis.Range.Max = gp.X.Invert ? plot_tl.x : plot_br.x;
        }
        for (int i = 0; i < MAX_Y_AXES; i++) {
            if (!gp.Y[i].Lock && plot.YAxis[i].Dragging) {
                ImPlotPoint plot_tl = PixelsToPlot(gp.BB_Plot.Min - IO.MouseDelta, i);
                ImPlotPoint plot_br = PixelsToPlot(gp.BB_Plot.Max - IO.MouseDelta, i);

                if (!gp.Y[i].LockMin)
                    plot.YAxis[i].Range.Min = gp.Y[i].Invert ? plot_tl.y : plot_br.y;
                if (!gp.Y[i].LockMax)
                    plot.YAxis[i].Range.Max = gp.Y[i].Invert ? plot_br.y : plot_tl.y;
            }
        }
        // Set the mouse cursor based on which axes are moving.
        int direction = 0;
        if (!gp.X.Lock && plot.XAxis.Dragging) {
            direction |= (1 << 1);
        }
        for (int i = 0; i < MAX_Y_AXES; i++) {
            if (!gp.Y[i].Present) { continue; }
            if (!gp.Y[i].Lock && plot.YAxis[i].Dragging) {
                direction |= (1 << 2);
                break;
            }
        }
        if (IO.MouseDragMaxDistanceSqr[0] > 5) {
            if (direction == 0) {
                ImGui::SetMouseCursor(ImGuiMouseCursor_NotAllowed);
            }
            else if (direction == (1 << 1)) {
                ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
            }
            else if (direction == (1 << 2)) {
                ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
            }
            else {
                ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);
            }
        }
    }
    // start drag
    if (!drag_in_progress && gp.Hov_Frame && IO.MouseClicked[gp.InputMap.PanButton] && HasFlag(IO.KeyMods, gp.InputMap.PanMod) && !plot.Selecting && !hov_legend && !hov_query && !plot.DraggingQuery) {
        if (hov_x_axis_region) {
            plot.XAxis.Dragging = true;
        }
        for (int i = 0; i < MAX_Y_AXES; i++) {
            if (hov_y_axis_region[i]) {
                plot.YAxis[i].Dragging = true;
            }
        }
    }

    // SCROLL INPUT -----------------------------------------------------------

    if (gp.Hov_Frame && (hov_x_axis_region || any_hov_y_axis_region) && IO.MouseWheel != 0) {
        UpdateTransformCache();
        float zoom_rate = 0.1f;
        if (IO.MouseWheel > 0)
            zoom_rate = (-zoom_rate) / (1.0f + (2.0f * zoom_rate));
        float tx = Remap(IO.MousePos.x, gp.BB_Plot.Min.x, gp.BB_Plot.Max.x, 0.0f, 1.0f);
        float ty = Remap(IO.MousePos.y, gp.BB_Plot.Min.y, gp.BB_Plot.Max.y, 0.0f, 1.0f);
        if (hov_x_axis_region && !gp.X.Lock) {
            ImPlotAxisScale axis_scale(0, tx, ty, zoom_rate);
            const ImPlotPoint& plot_tl = axis_scale.Min;
            const ImPlotPoint& plot_br = axis_scale.Max;

            if (!gp.X.LockMin)
                plot.XAxis.Range.Min = gp.X.Invert ? plot_br.x : plot_tl.x;
            if (!gp.X.LockMax)
                plot.XAxis.Range.Max = gp.X.Invert ? plot_tl.x : plot_br.x;
        }
        for (int i = 0; i < MAX_Y_AXES; i++) {
            if (hov_y_axis_region[i] && !gp.Y[i].Lock) {
                ImPlotAxisScale axis_scale(i, tx, ty, zoom_rate);
                const ImPlotPoint& plot_tl = axis_scale.Min;
                const ImPlotPoint& plot_br = axis_scale.Max;
                if (!gp.Y[i].LockMin)
                    plot.YAxis[i].Range.Min = gp.Y[i].Invert ? plot_tl.y : plot_br.y;
                if (!gp.Y[i].LockMax)
                    plot.YAxis[i].Range.Max = gp.Y[i].Invert ? plot_br.y : plot_tl.y;
            }
        }
    }

    //
    //
    // Begin cpp.cpp added stuff
    //
    //

    if (gp.NextPlotData.HasXRange) {
        plot.XAxis.Range.Min = ImMax(plot.XAxis.Range.Min, gp.NextPlotData.X.Min);
        plot.XAxis.Range.Max = ImMin(plot.XAxis.Range.Max, gp.NextPlotData.X.Max);
    }

    for (int i = 0; i < MAX_Y_AXES; i++) {
        if (gp.NextPlotData.HasYRange[i]) {
            plot.YAxis[i].Range.Min = ImMax(plot.YAxis[i].Range.Min, gp.NextPlotData.Y[i].Min);
            plot.YAxis[i].Range.Max = ImMin(plot.YAxis[i].Range.Max, gp.NextPlotData.Y[i].Max);
        }
    }

    //
    //
    // End cpp.cpp added stuff
    //
    //


    // BOX-SELECTION AND QUERY ------------------------------------------------

    // confirm selection
    if (plot.Selecting && (IO.MouseReleased[gp.InputMap.BoxSelectButton] || !IO.MouseDown[gp.InputMap.BoxSelectButton])) {
        UpdateTransformCache();
        ImVec2 select_size = plot.SelectStart - IO.MousePos;
        if (HasFlag(plot.Flags, ImPlotFlags_BoxSelect) && ImFabs(select_size.x) > 2 && ImFabs(select_size.y) > 2) {
            ImPlotPoint p1 = PixelsToPlot(plot.SelectStart);
            ImPlotPoint p2 = PixelsToPlot(IO.MousePos);
            if (!gp.X.LockMin && IO.KeyMods != gp.InputMap.HorizontalMod)
                plot.XAxis.Range.Min = ImMin(p1.x, p2.x);
            if (!gp.X.LockMax && IO.KeyMods != gp.InputMap.HorizontalMod)
                plot.XAxis.Range.Max = ImMax(p1.x, p2.x);
            for (int i = 0; i < MAX_Y_AXES; i++) {
                p1 = PixelsToPlot(plot.SelectStart, i);
                p2 = PixelsToPlot(IO.MousePos, i);
                if (!gp.Y[i].LockMin && IO.KeyMods != gp.InputMap.VerticalMod)
                    plot.YAxis[i].Range.Min = ImMin(p1.y, p2.y);
                if (!gp.Y[i].LockMax && IO.KeyMods != gp.InputMap.VerticalMod)
                    plot.YAxis[i].Range.Max = ImMax(p1.y, p2.y);
            }
        }
        plot.Selecting = false;
    }
    // bad selection
    if (plot.Selecting && (!HasFlag(plot.Flags, ImPlotFlags_BoxSelect) || gp.LockPlot) && ImLengthSqr(plot.SelectStart - IO.MousePos) > 4) {
        ImGui::SetMouseCursor(ImGuiMouseCursor_NotAllowed);
    }
    // cancel selection
    if (plot.Selecting && (IO.MouseClicked[gp.InputMap.BoxSelectCancelButton] || IO.MouseDown[gp.InputMap.BoxSelectCancelButton])) {
        plot.Selecting = false;
    }
    // begin selection or query
    if (gp.Hov_Frame && gp.Hov_Plot && IO.MouseClicked[gp.InputMap.BoxSelectButton] && HasFlag(IO.KeyMods, gp.InputMap.BoxSelectMod)) {
        plot.SelectStart = IO.MousePos;
        plot.Selecting = true;
    }
    // update query
    if (plot.Querying) {
        UpdateTransformCache();
        plot.QueryRect.Min.x = HasFlag(IO.KeyMods, gp.InputMap.HorizontalMod) ? gp.BB_Plot.Min.x : ImMin(plot.QueryStart.x, IO.MousePos.x);
        plot.QueryRect.Max.x = HasFlag(IO.KeyMods, gp.InputMap.HorizontalMod) ? gp.BB_Plot.Max.x : ImMax(plot.QueryStart.x, IO.MousePos.x);
        plot.QueryRect.Min.y = HasFlag(IO.KeyMods, gp.InputMap.VerticalMod) ? gp.BB_Plot.Min.y : ImMin(plot.QueryStart.y, IO.MousePos.y);
        plot.QueryRect.Max.y = HasFlag(IO.KeyMods, gp.InputMap.VerticalMod) ? gp.BB_Plot.Max.y : ImMax(plot.QueryStart.y, IO.MousePos.y);

        plot.QueryRect.Min -= gp.BB_Plot.Min;
        plot.QueryRect.Max -= gp.BB_Plot.Min;
    }
    // end query
    if (plot.Querying && (IO.MouseReleased[gp.InputMap.QueryButton] || IO.MouseReleased[gp.InputMap.BoxSelectButton])) {
        plot.Querying = false;
        if (plot.QueryRect.GetWidth() > 2 && plot.QueryRect.GetHeight() > 2) {
            plot.Queried = true;
        }
        else {
            plot.Queried = false;
        }
    }

    // begin query
    if (HasFlag(plot.Flags, ImPlotFlags_Query) && gp.Hov_Frame && gp.Hov_Plot && IO.MouseClicked[gp.InputMap.QueryButton] && HasFlag(IO.KeyMods, gp.InputMap.QueryMod)) {
        plot.QueryRect = ImRect(0,0,0,0);
        plot.Querying = true;
        plot.Queried  = true;
        plot.QueryStart = IO.MousePos;
    }
    // toggle between select/query
    if (HasFlag(plot.Flags, ImPlotFlags_Query) && plot.Selecting && HasFlag(IO.KeyMods,gp.InputMap.QueryToggleMod)) {
        plot.Selecting = false;
        plot.QueryRect = ImRect(0,0,0,0);
        plot.Querying = true;
        plot.Queried  = true;
        plot.QueryStart = plot.SelectStart;
    }
    if (HasFlag(plot.Flags, ImPlotFlags_BoxSelect) && plot.Querying && !HasFlag(IO.KeyMods, gp.InputMap.QueryToggleMod) && !IO.MouseDown[gp.InputMap.QueryButton]) {
        plot.Selecting = true;
        plot.Querying = false;
        plot.Queried = false;
        plot.QueryRect = ImRect(0,0,0,0);
    }

    // DOUBLE CLICK -----------------------------------------------------------

    if ( IO.MouseDoubleClicked[gp.InputMap.FitButton] && gp.Hov_Frame && (hov_x_axis_region || any_hov_y_axis_region) && !hov_legend && !hov_query) {
        gp.FitThisFrame = true;
        gp.FitX = hov_x_axis_region;
        for (int i = 0; i < MAX_Y_AXES; i++) {
            gp.FitY[i] = hov_y_axis_region[i];
        }
    }
    else {
        gp.FitThisFrame = false;
        gp.FitX = false;
        for (int i = 0; i < MAX_Y_AXES; i++) {
            gp.FitY[i] = false;
        }
    }

    // FOCUS ------------------------------------------------------------------

    // focus window
    if ((IO.MouseClicked[0] || IO.MouseClicked[1] || IO.MouseClicked[2]) && gp.Hov_Frame)
        ImGui::FocusWindow(ImGui::GetCurrentWindow());

    UpdateTransformCache();

    // set mouse position
    for (int i = 0; i < MAX_Y_AXES; i++) {
        gp.LastMousePos[i] = PixelsToPlot(IO.MousePos, i);
    }

    // RENDER -----------------------------------------------------------------

    // grid bg
    DrawList.AddRectFilled(gp.BB_Plot.Min, gp.BB_Plot.Max, gp.Col_Bg);

    // render axes
    PushPlotClipRect();

    // transform ticks
    if (gp.RenderX) {
        for (int t = 0; t < gp.XTicks.Size; t++) {
            ImPlotTick *xt = &gp.XTicks[t];
            xt->PixelPos = PlotToPixels(xt->PlotPos, 0, 0).x;
        }
    }
    for (int i = 0; i < MAX_Y_AXES; i++) {
        if (gp.RenderY[i]) {
            for (int t = 0; t < gp.YTicks[i].Size; t++) {
                ImPlotTick *yt = &gp.YTicks[i][t];
                yt->PixelPos = PlotToPixels(0, yt->PlotPos, i).y;
            }
        }
    }

    // render grid
    if (HasFlag(plot.XAxis.Flags, ImPlotAxisFlags_GridLines)) {
        for (int t = 0; t < gp.XTicks.Size; t++) {
            ImPlotTick *xt = &gp.XTicks[t];
            DrawList.AddLine(ImVec2(xt->PixelPos, gp.BB_Plot.Min.y), ImVec2(xt->PixelPos, gp.BB_Plot.Max.y), xt->Major ? gp.Col_X.Major : gp.Col_X.Minor, 1);
        }
    }

    for (int i = 0; i < MAX_Y_AXES; i++) {
        if (gp.Y[i].Present && HasFlag(plot.YAxis[i].Flags, ImPlotAxisFlags_GridLines)) {
            for (int t = 0; t < gp.YTicks[i].Size; t++) {
                ImPlotTick *yt = &gp.YTicks[i][t];
                DrawList.AddLine(ImVec2(gp.BB_Plot.Min.x, yt->PixelPos), ImVec2(gp.BB_Plot.Max.x, yt->PixelPos), yt->Major ? gp.Col_Y[i].Major : gp.Col_Y[i].Minor, 1);
            }
        }
    }

    PopPlotClipRect();

    // render title
    if (title_size.x > 0.0f) {
        ImGui::RenderText(ImVec2(gp.BB_Canvas.GetCenter().x - title_size.x * 0.5f, gp.BB_Canvas.Min.y), title, NULL, true);
    }

    // render labels
    if (HasFlag(plot.XAxis.Flags, ImPlotAxisFlags_TickLabels)) {
        ImGui::PushClipRect(gp.BB_Frame.Min, gp.BB_Frame.Max, true);
        for (int t = 0; t < gp.XTicks.Size; t++) {
            ImPlotTick *xt = &gp.XTicks[t];
            if (xt->RenderLabel && xt->PixelPos >= gp.BB_Plot.Min.x - 1 && xt->PixelPos <= gp.BB_Plot.Max.x + 1)
                DrawList.AddText(ImVec2(xt->PixelPos - xt->Size.x * 0.5f, gp.BB_Plot.Max.y + txt_off), gp.Col_X.Txt, gp.XTickLabels.Buf.Data + xt->TextOffset);
        }
        ImGui::PopClipRect();
    }
    if (x_label) {
        const ImVec2 xLabel_size = ImGui::CalcTextSize(x_label);
        const ImVec2 xLabel_pos(gp.BB_Plot.GetCenter().x - xLabel_size.x * 0.5f,
                                gp.BB_Canvas.Max.y - txt_height);
        DrawList.AddText(xLabel_pos, gp.Col_X.Txt, x_label);
    }
    ImGui::PushClipRect(gp.BB_Frame.Min, gp.BB_Frame.Max, true);
    for (int i = 0; i < MAX_Y_AXES; i++) {
        if (gp.Y[i].Present && HasFlag(plot.YAxis[i].Flags, ImPlotAxisFlags_TickLabels)) {
            for (int t = 0; t < gp.YTicks[i].Size; t++) {
                const float x_start = gp.AxisLabelReference[i] + (i == 0 ?  (-txt_off - gp.YTicks[i][t].Size.x) : txt_off);
                ImPlotTick *yt = &gp.YTicks[i][t];
                if (yt->RenderLabel && yt->PixelPos >= gp.BB_Plot.Min.y - 1 && yt->PixelPos <= gp.BB_Plot.Max.y + 1) {
                    ImVec2 start(x_start, yt->PixelPos - 0.5f * yt->Size.y);
                    DrawList.AddText(start, gp.Col_Y[i].Txt, gp.YTickLabels[i].Buf.Data + yt->TextOffset);
                }
            }
        }
    }
    ImGui::PopClipRect();
    if (y_label) {
        const ImVec2 yLabel_size = CalcTextSizeVertical(y_label);
        const ImVec2 yLabel_pos(gp.BB_Canvas.Min.x, gp.BB_Plot.GetCenter().y + yLabel_size.y * 0.5f);
        AddTextVertical(&DrawList, y_label, yLabel_pos, gp.Col_Y[0].Txt);
    }

    // PREP -------------------------------------------------------------------

    // push plot ID into stack
    ImGui::PushID(ID);
    return true;
}

} // namespace ImPlot
