/*

                          Firewall Builder

                 Copyright (C) 2003 NetCitadel, LLC

  Author:  Vadim Kurland     vadim@fwbuilder.org

  $Id$

  This program is free software which we release under the GNU General Public
  License. You may redistribute and/or modify this program under the terms
  of that license as published by the Free Software Foundation; either
  version 2 of the License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  To get a copy of the GNU General Public License, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

*/


#include "../../config.h"
#include "global.h"
#include "utils.h"
#include "platforms.h"

#include "FWWindow.h"
#include "FWBSettings.h"
#include "PrintingProgressDialog.h"
#include "PrintingController.h"
#include "ProjectPanel.h"

#include "fwbuilder/Firewall.h"
#include "fwbuilder/RuleSet.h"

#include <qglobal.h>

#include <QPrintDialog>
#include <QAbstractPrintDialog>
#include <QMessageBox>
#include <QStatusBar>
#include <QMdiArea>
#include <QMdiSubWindow>

using namespace libfwbuilder;
using namespace std;


void FWWindow::filePrint()
{
    if (!activeProject())
    {
        if (fwbdebug) qDebug("There isn't any selected subwindow");
        return;
    }

    int pageWidth = 0;
    int pageHeight = 0;
    bool  fullPage = false;

    float margin = 0;
    float table_scaling = 1.0;
    bool  print_header = true;
    bool  print_legend = true;
    bool  print_objects = true;
    bool  newPageForSection = false;
    int   tableResolution = 2;   // 50%, 75%, 100%, 150%, 200%, default 100%

    FWObject *firewall_to_print = NULL;
    FWObject *current_ruleset = activeProject()->getCurrentRuleSet();
    if (current_ruleset)
        firewall_to_print = current_ruleset->getParent();
    else
    {
        // no ruleset is open in the right panel
        firewall_to_print = activeProject()->getSelectedObject();
    }

    // Need error dialog
    if (!Firewall::isA(firewall_to_print)) return;


    if (!st->getStr("PrintSetup/newPageForSection").isEmpty())
        newPageForSection = st->getBool("PrintSetup/newPageForSection");

    if (!st->getStr("PrintSetup/printHeader").isEmpty())
        print_header = st->getBool("PrintSetup/printHeader");

    if (!st->getStr("PrintSetup/printLegend").isEmpty())
        print_legend = st->getBool("PrintSetup/printLegend");

    if (!st->getStr("PrintSetup/printObjects").isEmpty())
        print_objects = st->getBool("PrintSetup/printObjects");

    if (!st->getStr("PrintSetup/tableResolution").isEmpty())
    {
        tableResolution = st->getInt("PrintSetup/tableResolution");
        // for backwards compatibility, convert resolutino from an index
        // in a table to float 0..1.0
        // Previously values were from the following list:
        // 50%, 75%, 100%, 150%, 200%, default 100%
        float old_res[] = {50, 75, 100, 150, 200 };
        if (tableResolution <= 4  )
            tableResolution = old_res[int(tableResolution)];
    }

    QDialog dlg;

    psd = new Ui::pageSetupDialog_q();
    psd->setupUi(&dlg);
    connect(psd->tableResolution, SIGNAL(valueChanged(int)),
            this, SLOT(tableResolutionSettingChanged(int)));

    psd->newPageForSection->setChecked(newPageForSection);
    psd->printHeader->setChecked(print_header);
    psd->printLegend->setChecked(print_legend);
    psd->printObjects->setChecked(print_objects);
    psd->tableResolution->setValue(tableResolution);

    if ( dlg.exec() == QDialog::Accepted )
    {
        newPageForSection = psd->newPageForSection->isChecked();
        print_header = psd->printHeader->isChecked();
        print_legend = psd->printLegend->isChecked();
        print_objects = psd->printObjects->isChecked();
        tableResolution = psd->tableResolution->value();

        st->setBool("PrintSetup/newPageForSection", newPageForSection);
        st->setBool("PrintSetup/printHeader", print_header);
        st->setBool("PrintSetup/printLegend", print_legend);
        st->setBool("PrintSetup/printObjects", print_objects);
        st->setInt("PrintSetup/tableResolution", tableResolution);

        st->getPrinterOptions(printer, pageWidth, pageHeight);

        table_scaling = float(tableResolution) / 100;

        //printer->setResolution(resolution);
        printer->setFullPage(fullPage);

        QPrintDialog pdialog(printer, this);

        pdialog.addEnabledOption(QAbstractPrintDialog::PrintPageRange);
        pdialog.setMinMax(1,9999);
        pdialog.setPrintRange(QAbstractPrintDialog::AllPages);

        if (pdialog.exec())
        {
            int fromPage = printer->fromPage();
            int toPage = printer->toPage();
            if (fromPage==0) fromPage=1;
            if (toPage==0) toPage=9999;

            statusBar()->showMessage( "Printing..." );

            PrintingProgressDialog *ppd = 
                new PrintingProgressDialog(this, printer, 0, false);

            QString headerText = mw->printHeader();

#if defined(Q_OS_MACX)
            printerStream pr(printer, table_scaling, margin,
                             print_header, headerText, NULL);
#else
            printerStream pr(printer, table_scaling, margin,
                             print_header, headerText, ppd);
            ppd->show();
#endif
            pr.setFromTo(fromPage, toPage);

            if (fwbdebug)
                qDebug("Printer resolution: %d dpi", printer->resolution());

            if ( !pr.begin())
            {
                ppd->hide();
                delete ppd;
                return;
            }

            PrintingController prcontr(&pr);

            prcontr.printFirewall(firewall_to_print, activeProject());
            if (print_legend) prcontr.printLegend(newPageForSection);
            if (print_objects) prcontr.printObjects(firewall_to_print,
                                                    newPageForSection);

            ppd->hide();
            delete ppd;

            pr.end();

            if (printer->printerState() == QPrinter::Aborted)
            {
                statusBar()->showMessage( tr("Printing aborted"), 2000 );
                QMessageBox::information(
                    this,"Firewall Builder",
                    tr("Printing aborted"),
                    tr("&Continue"), QString::null,QString::null,
                    0, 1 );
            } else
                statusBar()->showMessage( tr("Printing completed"), 2000 );

        } else
        {
            statusBar()->showMessage( tr("Printing <ed"), 2000 );
        }

        st->setPrinterOptions(printer,pageWidth,pageHeight);
    }

    delete psd;
    psd = NULL;
}

void FWWindow::tableResolutionSettingChanged(int )
{
    if (psd)
    {
        QString res_lbl = QString("%1 %").arg(psd->tableResolution->value());
        psd->tableResolutionLabel->setText(res_lbl);
    }
}

void FWWindow::printFirewallFromFile(QString fileName,
                                     QString firewallName,
                                     QString outputFileName)
{
    if (outputFileName=="")
    {
        outputFileName = "print.pdf";
    }
    if (firewallName=="")
    {
        return ;
    }
    if (fileName=="")
    {
        return;
    }

    FWObjectDatabase * objdb = new FWObjectDatabase();
    QPrinter *printer = new QPrinter(QPrinter::HighResolution);
    objdb->load(fileName.toLatin1().constData(), NULL,librespath);
    FWObject* obj = objdb->findObjectByName(Firewall::TYPENAME,
                                            firewallName.toAscii().data());
    if (obj!=NULL)
    {
        int pageWidth = 0;
        int pageHeight = 0;
        bool  fullPage = false;
    
        float margin = 0;
        float table_scaling = 1.0;
        bool  print_header = true;
        bool  print_legend = true;
        bool  print_objects = true;
        bool  newPageForSection = false;
        int   tableResolution = 2;   // 50%, 75%, 100%, 150%, 200%, default 100%
    
        if (!st->getStr("PrintSetup/newPageForSection").isEmpty())
            newPageForSection = st->getBool("PrintSetup/newPageForSection");
    
        if (!st->getStr("PrintSetup/printHeader").isEmpty())
            print_header = st->getBool("PrintSetup/printHeader");
    
        if (!st->getStr("PrintSetup/printLegend").isEmpty())
            print_legend = st->getBool("PrintSetup/printLegend");
    
        if (!st->getStr("PrintSetup/printObjects").isEmpty())
            print_objects = st->getBool("PrintSetup/printObjects");
    
        if (!st->getStr("PrintSetup/tableResolution").isEmpty())
            tableResolution = st->getInt("PrintSetup/tableResolution");
    
        table_scaling = float(tableResolution) / 100;

        st->getPrinterOptions(printer,pageWidth,pageHeight);

        //printer->setResolution(resolution);
        printer->setFullPage(fullPage);
        printer->setOutputFileName (outputFileName);
        int fromPage = 1;
        int toPage = 9999;

        QString headerText = fileName; //mw->printHeader();
        printerStream pr(printer, table_scaling,
                         margin, print_header, headerText, NULL);

        pr.setFromTo(fromPage,toPage);

        if ( !pr.begin()) return;

        PrintingController prcontr(&pr);

        prcontr.printFirewall(obj, NULL);

        if (print_legend) prcontr.printLegend(newPageForSection);
        if (print_objects) prcontr.printObjects(obj, newPageForSection);
    }
    else
    {   
        QString errmes = "Error: can't find firewall ";
        errmes += firewallName ;
        qDebug (errmes.toAscii().data());
    }
}

