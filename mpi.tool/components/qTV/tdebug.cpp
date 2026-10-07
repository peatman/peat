/*
 * This file is part of PEAT (Performance Estimation & Analysing Toolkit).
 * Copyright (C) 2026 by Alex Knyazev
 *
 * PEAT is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * PEAT is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with PEAT.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <QApplication>
#include <QDir>
#include <QFile>
#include <QPainter>
#include <QtDebug>
#include <QtGlobal>

#include <QHeaderView>
#include <QProgressDialog>
#include <sstream>
#include <stdexcept>

#include "../common/common_types.h"
#include "elements.h"
#include "globals.h"
#ifdef _OPENMP
#include <omp.h>
#endif

#define heightPercent 1.0
#define showElementsThreshold 1 // 0.3
#define MAX_SIZE 100
#if 0
void Elements::draw_elements(QProgressBar* pb, QScrollBar* sr, QImage* finalImage,  QImage* threadImages[], int render_threads, const qreal startTime, const qreal stopTime)
{
    const QString progBarText = pb->format();
    std::vector<QPainter*> p;
    p.resize(render_threads);
    for (int a = 0; a < render_threads; a++) {
        p[a] = new QPainter(threadImages[a]);
        //p[a]->begin(threadImages[a]);
        p[a]->setRenderHint(QPainter::Antialiasing, true);
    }
    const qreal currentOpacity = 0.5;
    int wMinNameSize = p[0]->fontMetrics().maxWidth() >> 1;
    wMinNameSize *= strlen("MPI_ISEND");
    noDrawElems = 0;

    sr->hide();
    pb->setFormat("Rendering trace..." + progBarText + " done");
    pb->setMinimum(0);
    pb->setMaximum(get_draw_rank_count() + get_total_rank_count());
    pb->setValue(0);
    pb->show();
    QApplication::processEvents();
    //Draw functions (events)
    PEAT_Rank r = 0;
    long long prevclk = 0;
    int pb_value = 0;
    long s_width = paint.width;
    long thread_height= paint.thread_height;
    double pix_in_tic = s_width / (stopTime - startTime);
    double wMinNameH = (p[0]->fontMetrics().height()) * 0.8;//to prevent crossing MPI event rect and text. MPI Event will not be shown in this case.
#pragma omp parallel for num_threads(render_threads) schedule(dynamic) // firstprivate (rankPtext,funcPline,pePline,funcPtext,linePrank)
//   for (std::vector<std::vector<TV_Event*>>::const_iterator itRank = eventsV.begin(); itRank != eventsV.end(); ++itRank, ++r) {
    for (r = 0; r < ranks_to_load_v.size(); ++r) {
#ifdef _OPENMP
        int tid = omp_get_thread_num();
#else
        int tid = 0;
#endif
        assert(tid < render_threads);
        if (tid == 0) progress(prevclk, QApplication::processEvents);
        std::vector<TV_Event*>& itRank = eventsV.at(ranks_to_load_v.at(r));
        QPen   line_rank(Qt::black, 0, Qt::DashDotLine);
        //QPen   linePrank(Qt::black, 0, Qt::SolidLine);

        //p[tid]->setOpacity(currentOpacity);
        p[tid]->setOpacity(1.0);
//draw user code rect
        p[tid]->setPen(Qt::NoPen);
        p[tid]->setBrush(tv_colors.event_brushes[TV_BRUSH_EVENT_TYPE_USER_CODE]);
        long y_start = paint.mpi_ulc.at(r);
        long pix_in_rank = (get_mpi_threads(r) + get_omp_threads(r) + get_gpu_threads(r)) * paint.thread_height;
        p[tid]->drawRect(0, y_start, s_width, pix_in_rank);
        if (r && (r < get_draw_rank_count())) {
            p[tid]->setPen(line_rank);
            p[tid]->drawLine(0, r * pix_in_rank, s_width, r * pix_in_rank);
        }
        QColor user_region_color = threadImages[tid]->pixel(0, y_start);
        //p[tid]->setOpacity(currentOpacity);

        if (itRank.empty()) continue;
#pragma omp atomic
        pb_value++;
        if (tid == 0 ) {
            if (pb->value() != pb_value) pb->setValue(pb_value);
        }
        EventRankTypeConstIter initIt = lower_bound(itRank, startTime);
        if (initIt != itRank.begin()) {
            --initIt;
        }
        // long xEndOfPreviousMPIEvent = 0;
        for (EventRankTypeConstIter it = initIt; it != itRank.end(); ++it) {
            // long height = thread_height;
            const TV_Event* cur_func = *it;
            if (cur_func == NULL) {
                continue;
            }
            if (cur_func->e_time.func.out <= startTime) {
                continue;
            }
            if (cur_func->e_time.func.in >= stopTime) {
                break;
            }
            if (tid == 0) progress(prevclk, QApplication::processEvents);
            std::vector<const TV_Event *> subtree; // the call and its nested records at every level
            cur_func->collect_subtree(subtree);
            for (size_t f = 0; f < subtree.size(); f++) {
                const TV_Event *curFunc = subtree[f];
                long f_width = double(curFunc->e_time.func.duration()) * pix_in_tic;
                long pe_width = double(curFunc->e_time.pe.duration()) * pix_in_tic;
                if (pe_width == f_width) pe_width = 0;
                if (f_width < 1) {
                    f_width = 1;
                }
                long xStart = (curFunc->e_time.func.in - startTime) * pix_in_tic;
                if (xStart < 0) {
                    f_width += xStart;
                    xStart = 0;
                }
                long xPeStart = (curFunc->e_time.pe.in - startTime) * pix_in_tic;
                if (xPeStart < 0) {
                    if (pe_width > 0) pe_width += xPeStart;
                    xPeStart = 0;
                }
                //long yStart = ranks_to_load_v_rev[curFunc->rank] * pixInRank + (pixInRank - (heightPercent * pixInRank)) * 0.5; //base
                assert(ranks_to_load_v_rev[curFunc->rank] == r);
                PEAT_Rank cur_func_rank = ranks_to_load_v_rev[curFunc->rank];
                long yStart = thread_height * curFunc->thread;
                if (cur_func->is_mpi()) {
                    yStart += paint.mpi_ulc.at(cur_func_rank);
                } else {
                    if (cur_func->is_omp()) {
                        yStart += paint.omp_ulc.at(cur_func_rank);
                    } else {
                        if (cur_func->is_gpu()) {
                            yStart += paint.gpu_ulc.at(cur_func_rank);
                        }
                    }
                }
                p[tid]->setOpacity(1.0);
                QColor cc = threadImages[tid]->pixel(xStart, yStart);
                if (cc != user_region_color && (f_width < 2)) {
                    noDrawElems++;
                    continue;
                }
                //draw MPI event rect
                // QFont font = p[tid]->font();
                // qreal ps = font.pointSize();

                long height = thread_height;
                switch (curFunc->get_event_type()) {
                case TV_EVENT_TYPE_COLLECTIVE:
                    if (!shown->mpi_functions[2]) continue;
                case TV_EVENT_TYPE_COMM:
                    p[tid]->setBrush(tv_colors.event_brushes[TV_BRUSH_EVENT_TYPE_COLLECTIVE]);
                    break;
                case TV_EVENT_TYPE_TOR:
                    if (!shown->mpi_functions[1]) continue;
                    if (curFunc->check_tx()) {
                        p[tid]->setBrush(tv_colors.event_brushes[TV_BRUSH_EVENT_TYPE_SOURCE]);
                    }
                    if (curFunc->check_rx()) {
                        p[tid]->setBrush(tv_colors.event_brushes[TV_BRUSH_EVENT_TYPE_DESTINATION]);
                    }
                    break;
                case TV_EVENT_TYPE_TAR:
                    if (!shown->mpi_functions[1]) continue;
                    p[tid]->setBrush(tv_colors.event_brushes[TV_BRUSH_EVENT_TYPE_SAND]);
                    break;
                case TV_EVENT_TYPE_START:
                    if (!shown->mpi_functions[3]) continue;
                    p[tid]->setBrush(tv_colors.event_brushes[TV_BRUSH_EVENT_TYPE_START]);
                    break;
                case TV_EVENT_TYPE_COMPLETION:
                    if (!shown->mpi_functions[3]) continue;
                    p[tid]->setBrush(tv_colors.event_brushes[TV_BRUSH_EVENT_TYPE_COMPLETION]);
                    break;
                case TV_EVENT_TYPE_GPU_TRANSFER:
                    if (!shown->gpu_functions[1]) continue;
                    p[tid]->setBrush(tv_colors.event_brushes[TV_BRUSH_EVENT_TYPE_GPU]);
                    break;
                case TV_EVENT_TYPE_GPU_LOCAL:
                    if (!shown->gpu_functions[0]) continue;
                    p[tid]->setBrush(tv_colors.event_brushes[TV_BRUSH_EVENT_TYPE_GPU]);
                    break;
                case TV_EVENT_TYPE_OMPT_TARGET_DATA:
                    if (!shown->omp_functions[3]) continue;
                    // p[tid]->setFont(font);
                    p[tid]->setBrush(tv_colors.event_brushes[TV_BRUSH_EVENT_TYPE_OMPT_LOCAL]);
                    break;
                case TV_EVENT_TYPE_OMPT_LOCAL:
                    if (!shown->omp_functions[0]) continue;
                    // p[tid]->setFont(font);
                    p[tid]->setBrush(tv_colors.event_brushes[TV_BRUSH_EVENT_TYPE_OMPT_LOCAL]);
                    break;
                case TV_EVENT_TYPE_OMPT_PARALLEL:
                    if (!shown->omp_functions[1]) continue;
                    p[tid]->setBrush(tv_colors.event_brushes[TV_BRUSH_EVENT_TYPE_OMPT_PARALLEL]);
                    height = thread_height * get_omp_threads(r);
                    break;
                default:
                    p[tid]->setBrush(tv_colors.event_brushes[TV_BRUSH_EVENT_TYPE_OTHER]);
                    break;
                }
                // font.setPixelSize(height);
                QRect rfrom(xStart, yStart, f_width, height);
                QRect rpefrom(xPeStart, yStart, pe_width, height);
                p[tid]->setPen(tv_colors.func_line);
                p[tid]->setOpacity(currentOpacity);
                p[tid]->drawRect(rfrom);
                if (pe_width > 1) {
                    p[tid]->setBrush(tv_colors.event_brushes[TV_BRUSH_EVENT_TYPE_PE]);
                    p[tid]->setPen(tv_colors.pe_line);
                    p[tid]->drawRect(rpefrom);
                }

                //draw MPI event text
                if (f_width > wMinNameSize) {
                    p[tid]->setOpacity(1.0);
                    p[tid]->setPen(tv_colors.func_text);
                    p[tid]->drawText(rfrom, Qt::AlignLeft | Qt::AlignVCenter, QString(global_func_id.name_by_idx(curFunc->func_id)) + QString("\nidx:") + QString().setNum(curFunc->ids[0])); //Slow!
                }
                /*
                font.setPointSize(ps);
                p[tid]->setFont(font);
                
                //draw user region function name
                const long startFunctionNameBefore = qMax(long(0), xEndOfPreviousMPIEvent);
                const long widthFunctionNameBefore = xStart - startFunctionNameBefore;
                if (widthFunctionNameBefore > wMinNameSize) {
                    QString address1Name;
                    const PEAT_Addr address1 = curFunc->addrs[0].addr;
                    if (address1) {
                        address1Name.asprintf("%p", address1);
                    }
                    p[tid]->setPen(funcPtext);
                    p[tid]->setOpacity(1.0);
                    QRect userFunctionBefore(startFunctionNameBefore, yStart, widthFunctionNameBefore, thread_height);
                    p[tid]->drawText(userFunctionBefore, Qt::AlignRight | Qt::AlignVCenter, address1Name); //Slow!
                    p[tid]->setOpacity(currentOpacity);//restore opacity
                }
                */
                //save previous MPI function pixel end
                // if (curFunc == cur_func) xEndOfPreviousMPIEvent = xStart + width;
            }
        }
        p[tid]->setOpacity(1.0);
        p[tid]->setPen(tv_colors.rank_text);
        QString rstr = "R" + QString::number(ranks_to_load_v[r]);
        p[tid]->drawText(5, paint.mpi_ulc.at(r) + wMinNameH, rstr);
        p[tid]->setOpacity(currentOpacity);
    }
    QPainter f(finalImage);
    for (int i = 0; i < render_threads; ++i) {
        f.drawImage(0, 0, *threadImages[i]);
    }
    for (int a = 0; a < render_threads; a++) threadImages[a]->fill(Qt::transparent);
//Draw messages
    std::vector < qint32> y_offsets;
#define y_indent 5
#define y_inc 4
    PEAT_Rank num_ranks = get_total_rank_count();
    y_offsets.resize(num_ranks);
    PEAT_Rank cur_rank = 0;
#pragma omp parallel for num_threads(render_threads) schedule(dynamic)
    for (cur_rank = 0; cur_rank < messages.size(); ++cur_rank) {
    //for (std::vector<std::map<PEAT_Clock, TV_Message> >::const_iterator itRank = messages.begin(); itRank != messages.end(); ++itRank, ++cur_rank) {
#ifdef _OPENMP
        int tid = omp_get_thread_num();
#pragma omp atomic
        pb_value++;
#else
        int tid = 0;
#endif
        assert(tid < render_threads);
        if (tid == 0) {
            if (pb->value() != pb_value) pb->setValue(pb_value);
        }
        std::map<PEAT_Clock, TV_Message>& itRank = messages.at(cur_rank);
        if (tid == 0) progress(prevclk, QApplication::processEvents);
        if (itRank.empty()) continue;
        // size_t size = itRank.size();
        PEAT_LNum mess_num = 0;
        std::map<PEAT_Clock, TV_Message>::const_iterator initIt = itRank.lower_bound(startTime);
        for (std::map<PEAT_Clock, TV_Message>::const_iterator itMsg = initIt; itMsg != itRank.end(); ++itMsg) {
            // long height = rank_height;
            const TV_Event* from = itMsg->second.from;
            const TV_Event* to = itMsg->second.to;
            long from_thread_height = thread_height;
            long to_thread_height = thread_height;
            mess_num++;
            if (to->e_time.func.out <= startTime) {
                continue;
            }
            if (from->e_time.func.in >= stopTime && itMsg->second.msg_type != TV_PEN_MESSAGE_TYPE_REVERSED) {
                break;
            }
            if (tid == 0) progress(prevclk, QApplication::processEvents);
            //long wdtFrom= qAbs(from.timeOut-from.timeIn)*pixInTic;
            qint32 y_offset = y_offsets[ranks_to_load_v_rev[from->rank]];
            y_offsets[ranks_to_load_v_rev[from->rank]] += y_inc;
            y_offset %= (thread_height - y_indent);
            y_offset += y_indent;
            long wdtFrom = from->e_time.pe.duration() * pix_in_tic;
            long xFrom = (from->e_time.pe.in - startTime) * pix_in_tic;
            long yFrom = y_offset + from_thread_height * from->thread;
            long wdtTo = to->e_time.pe.duration() * pix_in_tic;
            long xTo = (to->e_time.pe.in - startTime) * pix_in_tic;
            long yTo = y_offset + to_thread_height * to->thread;
            if ((xFrom > 0 && xFrom < s_width) || (xTo + wdtTo > 0 && xTo + wdtTo < s_width)) {
                //Do not draw a line with 1 pix width
                p[tid]->setPen(tv_colors.message_pens[itMsg->second.msg_type]);
                switch (itMsg->second.msg_type) {
                case TV_PEN_MESSAGE_TYPE_NORMAL:
                    p[tid]->setPen(tv_colors.message_pens[TV_PEN_MESSAGE_TYPE_NORMAL]);
                    goto cont_noraml;
                case TV_PEN_MESSAGE_TYPE_REVERSED:
                    p[tid]->setPen(tv_colors.message_pens[TV_PEN_MESSAGE_TYPE_REVERSED]);
                    cont_noraml:
                    if (!shown->mpi_functions[1] || !shown->mpi_messages[0] ) continue;
                    if (qAbs(xTo + wdtTo - xFrom) <= showElementsThreshold) {
                        noDrawElems++;
                        continue;
                    }
                    yFrom += paint.mpi_ulc.at(ranks_to_load_v_rev[from->rank]);
                    yTo   += paint.mpi_ulc.at(ranks_to_load_v_rev[to->rank]);
                    yFrom -= y_offset; //always start from center 
                    yTo -= y_offset;  //always start from center 
                    p[tid]->drawLine(xFrom, yFrom + from_thread_height * 0.5, xTo + wdtTo, yTo + to_thread_height * 0.5);
                    //Point at the end of the line
                    p[tid]->drawPoint(xFrom, yFrom + from_thread_height * 0.5);
                    p[tid]->drawPoint(xTo + wdtTo, yTo + to_thread_height * 0.5);
                    break;
                case TV_PEN_MESSAGE_TYPE_GPU_HD_TRANSFER:
                case TV_PEN_MESSAGE_TYPE_GPU_EXEC:
                    yFrom += paint.gpu_ulc.at(ranks_to_load_v_rev[from->rank]);
                    yTo += paint.gpu_ulc.at(ranks_to_load_v_rev[to->rank]);
                    p[tid]->drawLine(xFrom + wdtFrom * 0.5, yFrom, xTo + wdtTo * 0.5, yTo);
                    // p[tid]->setPen(message_pens[TV_PEN_MESSAGE_TYPE_NONE]);
                    p[tid]->drawPoint(xFrom + wdtFrom * 0.5, yFrom);
                    p[tid]->drawPoint(xTo + wdtTo * 0.5, yTo);
                    break;
                case TV_PEN_MESSAGE_TYPE_INITIATION:
                case TV_PEN_MESSAGE_TYPE_COMPLETION:
                    if ((!shown->mpi_messages[2] || !shown->mpi_functions[3]) && itMsg->second.msg_type == TV_PEN_MESSAGE_TYPE_INITIATION) continue;
                    if ((!shown->mpi_messages[3] || !shown->mpi_functions[3]) && itMsg->second.msg_type == TV_PEN_MESSAGE_TYPE_COMPLETION) continue;
                    if (qAbs(xTo + wdtTo - xFrom) <= showElementsThreshold) {
                        noDrawElems++;
                        continue;
                    }
                    yFrom += paint.mpi_ulc.at(ranks_to_load_v_rev[from->rank]);
                    yTo += paint.mpi_ulc.at(ranks_to_load_v_rev[to->rank]);
                    p[tid]->drawLine(xFrom + wdtFrom * 0.5, yFrom, xTo + wdtTo * 0.5, yTo);
                    //p[tid]->setPen(message_pens[TV_PEN_MESSAGE_TYPE_NONE]);
                    p[tid]->drawPoint(xFrom + wdtFrom * 0.5, yFrom);
                    p[tid]->drawPoint(xTo + wdtTo * 0.5, yTo);
                    break;
                case TV_PEN_MESSAGE_TYPE_COLLECTIVE:
                    if (!shown->mpi_messages[1] || !shown->mpi_functions[2]) continue;
                    yFrom += paint.mpi_ulc.at(ranks_to_load_v_rev[from->rank]);
                    yTo += paint.mpi_ulc.at(ranks_to_load_v_rev[to->rank]);
                    yFrom -= y_offset; //always start from center 
                    yTo -= y_offset;  //always start from center 
                    if (qAbs(xTo + wdtTo * 0.5 - xFrom + wdtFrom * 0.5) <= showElementsThreshold) {
                        noDrawElems++;
                        continue;
                    }
                    p[tid]->setPen(tv_colors.message_pens[TV_PEN_MESSAGE_TYPE_COLLECTIVE]);
                    p[tid]->drawLine(xFrom + wdtFrom * 0.5, yFrom + from_thread_height * 0.5, xTo + wdtTo * 0.5, yTo + to_thread_height * 0.5);
                    //Point at the end of the line
                    //p[tid]->setPen(message_pens[TV_PEN_MESSAGE_TYPE_NONE]);
                    p[tid]->drawPoint(xFrom + wdtFrom * 0.5, yFrom + from_thread_height * 0.5);
                    p[tid]->drawPoint(xTo + wdtTo * 0.5, yTo + to_thread_height * 0.5);
                    break;
                default:
                    qDebug() << "Unknown Message Type xFrom " << xFrom << " xTo " << xTo + wdtTo;
                    break;
                }
            }
        }
    }

    for (int a = 0; a < render_threads; a++) {
        // p[a]->end();
        delete p[a];
    }

    for (int i = 0; i < render_threads; ++i) {
        f.drawImage(0, 0, *threadImages[i]);
    }

    pb->hide();
    sr->show();
    pb->setFormat(progBarText);//revert original text
    QApplication::processEvents();
    //qDebug()<< "No drew elements" << noDrawElems << "from" << fsize() << msize();
}
#endif
