/*******************
 *
 *
 *  GRATE 8
 *
 *  Graphical Interface
 *
 *
 *
*********************/


#include "mainwindow.h"
#include "model.h"
#include "ui_RwaveWin.h"
#include "tinyxml2/tinyxml2.h"
#include <stdlib.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <QFileDialog>
#include <QString>
#include <QDir>
#include <QMessageBox>
#include <ciso646>

#define PI 3.14159265

using namespace tinyxml2;

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow),
    initialised(false)
{
    // setup user interface
    ui->setupUi(this);

    // default input file name
    std::string param_file = "test_out.xml";

    // default output file
    QString a = QCoreApplication::applicationDirPath(); //QDir::currentPath();
    QString b = "/ModelResults.txt";
    ui->outputFileName->setText(a+b);

    setWindowTitle("GRATE Model");
    ui->textFileName->setText(param_file.c_str());
    ui->VectorPlot->replot();
    ui->startButton->setEnabled(0);     // Turn off start button until file is loaded.

    connect(ui->action_load_XML, SIGNAL(triggered()), this, SLOT(loadXML()));
}

void MainWindow::loadXML()
{
    std::string param_file;
    QString output_file;

    // check if file exists
    if (not std::fstream(param_file)) {
        std::cerr << "File does not exist... Need to specify..." << std::endl;
        QString fileName = QFileDialog::getOpenFileName(this, "Load Parameter File",
                                                        QDir::currentPath(),
                                                        "XML files (*.xml);;All files (*)");
        if (not fileName.isEmpty())
            param_file = fileName.toStdString();
    }

    // load xml input file
    std::cout << "Reading xml file: '" << param_file << "'" << std::endl;
    XMLDocument xml_params;
    if (xml_params.LoadFile(param_file.c_str()) != XML_SUCCESS) {
        std::cerr << "Error reading xml parameters:" << std::endl;
        std::cerr << xml_params.ErrorStr() << std::endl;
        // just show error message if file wasn't loaded
        std::stringstream error_stream;
        error_stream << "Error loading parameter file - try restarting and specifying the correct file";
        error_stream << std::endl << std::endl << xml_params.ErrorStr();
        showErrorMessage("Error loading parameter file", error_stream);
    }
    else {
        // file was loaded so initialise everything

        // get the root element of the XML document
        XMLElement *params_root = xml_params.FirstChildElement();
        if (params_root == NULL) {
            std::cerr << "Error getting root element from XML file" << std::endl;
            std::cerr << xml_params.ErrorStr() << std::endl;
            // show dialog with error message
            std::stringstream error_stream;
            error_stream << "Error reading root element from XML file - check file format";
            error_stream << std::endl << std::endl << xml_params.ErrorStr();
            showErrorMessage("Error reading root element from XML file", error_stream);
        }
        else {
            // initialise components
            try {
                output_file = ui->outputFileName->toPlainText();
                model = new Model(params_root, output_file.toStdString());

                // do other bits here
                setupChart();                                // Setup GUI graph
                setWindowTitle("GRATE Model");
                ui->textFileName->setText(param_file.c_str());
                ui->VectorPlot->replot();

                // Use <Refresh Plot> button to start run
                connect(ui->startButton, SIGNAL(clicked()), this, SLOT(kernel()), Qt::QueuedConnection);

                initialised = true;
            }
            catch (std::string msg) {
                std::cerr << "Error while initialising components: " << msg << std::endl;
                std::stringstream error_stream;
                error_stream << "Error while initialising components" << std::endl << std::endl << msg;
                showErrorMessage("Error initialising components", error_stream);
            }
        }
    }

    setupChart();                                // Setup GUI graph
    ui->startButton->setEnabled(1);
    ui->loadingAdvice->setVisible(0);
}

void MainWindow::showErrorMessage(const char *title, std::stringstream &msg_stream) {
    std::string msg = msg_stream.str();
    QMessageBox messageBox;
    messageBox.critical(this, title, msg.c_str());
}

void MainWindow::showInfoMessage(const char *title, std::stringstream &msg_stream) {
    std::string msg = msg_stream.str();
    QMessageBox messageBox;
    messageBox.information(this, title, msg.c_str());
}


void MainWindow::setupChart(){

    int i, j, k, n = 0;
    float theta_rad, a, b;
    unsigned int bc = 0;
    QString output_file;
    RiverProfile* rn = model->rn;
    hydro *wl = model->wl;
    sed *sd = model->sd;
    int nQw = wl->Qw[0].size();
    QVector<double> x( rn->nnodes );
    QVector<double> eta( rn->nnodes );
    QVector<double> bedrock( rn->nnodes );
    QVector<double> WSL( rn->nnodes );
    QVector<double> Froude( rn->nnodes );
    QVector<double> Bedload( rn->nnodes );
    QVector<double> Qw_Plot( rn->nnodes );
    QVector<double> LeftBankLower( rn->nnodes );
    QVector<double> RightBankLower( rn->nnodes );
    QVector<double> LeftBankTop( rn->nnodes );
    QVector<double> RightBankTop( rn->nnodes );
    QVector<double> XsPlotX( 11 );
    QVector<double> XsPlotY( 11 );
    QVector<double> wsXS_X( 2 );
    QVector<double> wsXS_Y( 2 );
    QVector<double> Bottom_X( 2 );
    QVector<double> Bottom_Y( 2 );
    QVector<double> Qw_TS( 900 );  //nQw );
    QVector<double> time( 900 );  //nQw );
    QVector<double> CursorX( 2 );
    QVector<double> CursorY( 2 );
    QVector<double> tmp( 8 );
    QVector<QVector<double> > GSD_Data, GSD_Cumul;

    rn->counter = 0;
    rn->yearCounter = 0;
    rn->writeInterval = ui->writeInt_disp->value();

    tmp.fill( 1, rn -> nnodes );
    GSD_Data.fill( tmp, 7);
    GSD_Cumul.fill(tmp, 13);

    for ( i = 0; i < rn -> nnodes; i++ )
    {
        x[i] = rn->RiverXS[i].node;
        eta[i] = rn->eta[i];
        bedrock[i] = rn->bedrock[i];
        WSL[i] = rn -> eta[i] + rn->RiverXS[i].depth * 8;    // x8 exaggeration for display
        Froude[i] = rn->ntop[i];                             // RiverXS[i].ustar;
        Bedload[i] = sd->Qs[i];
        Qw_Plot[i] = wl->QwCumul[i] / 100;
        for ( j = 0; j < rn->ngsz; j++ )                     // Make a cumulative dist
        {
            if ( j > 0 )
                GSD_Cumul[j][i] = GSD_Cumul[j-1][i];
            for ( k = 0; k < rn->nlith; k++  )
                GSD_Cumul[j][i] -= rn->F[i].pct[k][j];
            if (GSD_Cumul[j][i] < 0.0001)
                GSD_Cumul[j][i] = 0;
        }
    }

    // dt control
    ui->dt_disp->setValue(rn->dt);
    ui->deltaT->setValue(rn->dt);

    // upwind control
    ui->sedUpw_slider->setValue(rn->sedUpw * 100 );
    ui->hydroUpw_slider->setValue(rn->hydroUpw * 100 );

    // Setup cross-section graph
    n = ui->spinNode->value();
    theta_rad = rn->RiverXS[n].theta * PI / 180.;
    a = rn->RiverXS[n].bankHeight - rn->RiverXS[n].Hmax;  // Vert & Horiz triangle segments at lower channel.
    b = a / tan(theta_rad);

    XsPlotX[2] = -1.5 * rn->RiverXS[n].fpSlope;
    XsPlotY[2] = 0;
    XsPlotX[3] = 0;
    XsPlotY[3] = -1.5;
    XsPlotX[4] = 0.001;
    XsPlotY[4] = -1.5 - rn->RiverXS[n].Hmax;
    XsPlotX[5] = b;
    XsPlotY[5] = -1.5 - rn->RiverXS[n].bankHeight;
    XsPlotX[6] = b + rn->RiverXS[n].width;
    XsPlotY[6] = XsPlotY[5];
    XsPlotX[7] = XsPlotX[6] + b;
    XsPlotY[7] = XsPlotY[4];
    XsPlotX[8] = XsPlotX[7] + 0.001;
    XsPlotY[8] = -1.5;
    XsPlotX[9] = XsPlotX[8] + 1.5;
    XsPlotY[9] = 0;
    XsPlotX[10] = XsPlotX[9] + 5 * rn->RiverXS[n].valleyWallSlp;
    XsPlotY[10] = 5;

    XsPlotX[1] = XsPlotX[9] - rn->RiverXS[n].fpWidth;
    XsPlotY[1] = 0;
    XsPlotX[0] = XsPlotX[1] - ( 5 * rn->RiverXS[n].valleyWallSlp );
    XsPlotY[0] = 5;

    wsXS_X[0] = 0;
    wsXS_X[1] = XsPlotX[8];
    wsXS_Y[0] = -1.5;
    wsXS_Y[1] = -1.5;

    Bottom_X[0] = XsPlotX[0];
    Bottom_X[1] = XsPlotX[10];
    Bottom_Y[0] = -30;
    Bottom_Y[1] = -30;

    ui->spinNode->setMaximum(rn->nnodes);

    //Temporarily out of action; this will eventually be a tweak/time series switch

    //  for ( bc = 0; bc < 899; bc++ )
    //  {
    //      Qw_TS[bc] = wl->Qw[0][0].Q * rn->tweakArray[bc];
    //      time[bc] = bc;// * 3600;
    //  }

    for ( bc = 0; bc < nQw; bc++ )
    {
        Qw_TS[bc] = wl->Qw[0][bc].Q;
        time[bc] = double ( wl->Qw[0][0].date_time.secsTo(wl->Qw[0][bc].date_time) ) / 3600.;  // Time in hours
    }

    for ( j = 0; j < 7 ; j++ )
        for ( i = 0; i < rn->nnodes; i++ )
                GSD_Data[j][i] = GSD_Cumul[j*2][i];

    CursorX[0] = (wl->Qw[0][0].date_time.secsTo(rn->cTime)) / 3600.;
    CursorX[1] = CursorX[0];
    CursorY[0] = 0;
    CursorY[1] = 1200;

    ui->VectorPlot->xAxis->setLabel("Distance Downstream");
    ui->VectorPlot->yAxis->setLabel("Elevation");
    ui->VectorPlot->xAxis->setRange(0, rn->nnodes + 1);
    ui->VectorPlot->yAxis->setRange(round(rn->eta[rn->nnodes-1]-10), round(rn->eta[1]+20));

    ui->VectorPlot->addGraph();  // Orig channel bed elevation plot (static)
    ui->VectorPlot->graph(0)->setData( x, eta );
    ui->VectorPlot->graph(0)->setPen(QPen(Qt::gray));

    ui->VectorPlot->addGraph();  // Bedrock plot (static)
    QPen grayDotPen;
    grayDotPen.setColor(QColor(Qt::gray));
    grayDotPen.setStyle(Qt::DashLine);
    grayDotPen.setWidthF(1);
    ui->VectorPlot->graph(1)->setData( x, bedrock );
    ui->VectorPlot->graph(1)->setPen(grayDotPen);

    ui->VectorPlot->addGraph();  // Channel bed elevation plot
    ui->VectorPlot->graph(2)->setData( x, eta );
    ui->VectorPlot->graph(2)->setPen(QPen(Qt::black));
    ui->VectorPlot->graph(2)->setBrush(QColor(255, 161, 0, 50));

    ui->VectorPlot->addGraph();  // Water surface plot
    ui->VectorPlot->graph(3)->setData( x, WSL );
    ui->VectorPlot->graph(3)->setPen(QPen(Qt::blue));
    ui->VectorPlot->graph(3)->setBrush(QBrush(QColor(0, 0, 255, 20)));
    ui->VectorPlot->graph(3)->setChannelFillGraph(ui->VectorPlot->graph(1));
                                          //(rand() % 254, rand() % 254, rand() % 254)));

    ui->BedloadPlot->xAxis->setLabel("Distance Downstream");
    ui->BedloadPlot->yAxis->setLabel("Qs - Bedload Discharge");
    ui->BedloadPlot->xAxis->setRange(0,rn->nnodes+1);
    ui->BedloadPlot->yAxis->setRange(0, 15);

    ui->BedloadPlot->addGraph();  // Bedload plot
    ui->BedloadPlot->graph(0)->setData( x, Bedload );
    ui->BedloadPlot->graph(0)->setPen(QPen(Qt::black));

    ui->BedloadPlot->addGraph();  // Qw plot
    ui->BedloadPlot->graph(1)->setData( x, Qw_Plot );
    ui->BedloadPlot->graph(1)->setPen(QPen(Qt::blue));

    // BankWidth
    ui->BankWidthPlot->xAxis->setLabel("Distance Downstream");
    ui->BankWidthPlot->yAxis->setLabel("Bank Width");
    ui->BankWidthPlot->xAxis->setRange(0,rn->nnodes + 1);
    ui->BankWidthPlot->yAxis->setRange(-60,120);

    ui->BankWidthPlot->addGraph();  // Left Lower Bank
    ui->BankWidthPlot->graph(0)->setData( x, LeftBankLower );
    ui->BankWidthPlot->graph(0)->setPen(QPen(Qt::black));

    ui->BankWidthPlot->addGraph();  // Right Lower Bank
    ui->BankWidthPlot->graph(1)->setData( x, RightBankLower );
    ui->BankWidthPlot->graph(1)->setPen(QPen(Qt::black));

    ui->BankWidthPlot->addGraph();  // Left Upper Bank
    ui->BankWidthPlot->graph(2)->setData( x, LeftBankTop );
    ui->BankWidthPlot->graph(2)->setPen(QPen(Qt::black));

    ui->BankWidthPlot->addGraph();  // Right Upper Bank
    ui->BankWidthPlot->graph(3)->setData( x, RightBankTop );
    ui->BankWidthPlot->graph(3)->setPen(QPen(Qt::black));

    ui->XSectPlot->xAxis->setLabel("Cross Section Width");
    ui->XSectPlot->yAxis->setLabel("Relative Elevation");
    ui->XSectPlot->xAxis->setRange(-(rn->RiverXS[n].fpWidth / 2), rn->RiverXS[n].fpWidth / 2 );
    ui->XSectPlot->yAxis->setRange(-10, 12);

    ui->XSectPlot->addGraph();  // Cross-sectional plot
    ui->XSectPlot->graph(0)->setData( XsPlotX, XsPlotY );
    ui->XSectPlot->graph(0)->setPen(QPen(Qt::black));

    ui->XSectPlot->addGraph();  // Water surface plot
    ui->XSectPlot->graph(1)->setData( wsXS_X, wsXS_Y );
    ui->XSectPlot->graph(1)->setPen(QPen(Qt::blue));
    ui->XSectPlot->graph(1)->setBrush(QBrush(QColor(0, 0, 255, 20)));
    ui->XSectPlot->graph(1)->setChannelFillGraph(ui->XSectPlot->graph(0));

    ui->XSectPlot->addGraph();  // Bottom of cross-section (fill)
    ui->XSectPlot->graph(2)->setData( Bottom_X, Bottom_Y );
    ui->XSectPlot->graph(2)->setPen(QPen(Qt::black));
    ui->XSectPlot->graph(2)->setBrush(QColor(255, 161, 0, 50));
    ui->XSectPlot->graph(2)->setChannelFillGraph(ui->XSectPlot->graph(0));

    ui->QwSeries->xAxis->setTickLabelType(QCPAxis::ltDateTime);
    ui->QwSeries->yAxis->setLabel("Discharge");
    ui->QwSeries->xAxis->setLabel("Time");
    ui->QwSeries->xAxis->setRange( time[0], time[nQw-1] );  //time[899] );   Time is displayed in hours
    ui->QwSeries->yAxis->setRange(round(*min_element(Qw_TS.constBegin(),
            Qw_TS.constEnd()) * 0.75), round(*max_element(Qw_TS.constBegin(), Qw_TS.constEnd()) * 1.5));

    ui->QwSeries->addGraph();
    ui->QwSeries->graph(0)->setData( time, Qw_TS );
    ui->QwSeries->addGraph();
    ui->QwSeries->graph(1)->setData(CursorX, CursorY);
    ui->QwSeries->graph(1)->setPen(QPen(Qt::red));

    ui->GSD_Dash->xAxis->setLabel("Distance Downstream");
    ui->GSD_Dash->yAxis->setLabel("Cum %");
    ui->GSD_Dash->xAxis->setRange(0, rn->nnodes + 1);
    ui->GSD_Dash->yAxis->setRange(0, 1);

    ui->GSD_Dash->addGraph();  // Grain size distributions
    ui->GSD_Dash->graph(0)->setData( x, GSD_Data[0] );
    ui->GSD_Dash->addGraph();
    ui->GSD_Dash->graph(1)->setData( x, GSD_Data[1] );
    ui->GSD_Dash->addGraph();
    ui->GSD_Dash->graph(2)->setData( x, GSD_Data[2] );
    ui->GSD_Dash->addGraph();
    ui->GSD_Dash->graph(3)->setData( x, GSD_Data[3] );
    ui->GSD_Dash->addGraph();
    ui->GSD_Dash->graph(4)->setData( x, GSD_Data[4] );
    ui->GSD_Dash->addGraph();
    ui->GSD_Dash->graph(5)->setData( x, GSD_Data[5] );
    ui->GSD_Dash->addGraph();
    ui->GSD_Dash->graph(6)->setData( x, GSD_Data[6] );
    ui->GSD_Dash->graph(0)->setPen(QPen(QColor(0,0,0)));  // 0 is the finest grain size category: black
    for ( i = 1; i < 7 ; i++ )
           ui->GSD_Dash->graph(i)->setPen(QPen(QColor((i * 30), 254 - (i * 30), 113)));

    ui->grateDateTime->setDateTime(QDateTime::fromTime_t(rn->cTime.getTime_t() + 50000));
    ui->reportStep->setValue(rn->counter);
    //ui->reportYear->setValue(rn->yearCounter);

    ui->spinBankHt->setValue(rn->RiverXS[n].bankHeight);
    ui->spinTheta->setValue(rn->RiverXS[n].theta);
    ui->spinDepth->setValue(rn->RiverXS[n].depth);
    ui->spinNoChnl->setValue(rn->RiverXS[n].noChannels);
    ui->spinD50->setValue(pow(2, rn->F[n].dsg));
    ui->spinDcomp->setValue(pow(2, rn->RiverXS[n].comp_D));
    ui->spinHmax->setValue(rn->RiverXS[n].Hmax);

    ui->VectorPlot->replot();
    ui->BedloadPlot->replot();
    ui->BankWidthPlot->replot();
}

void MainWindow::kernel(){

    ui->grateDateTime->setDateTime(QDateTime::fromTime_t(model->rn->cTime.getTime_t() + 50000));

    connect(&dataTimer, SIGNAL(timeout()), this, SLOT( modelUpdate()) );
    connect(&dataTimer, SIGNAL(timeout()), this, SLOT( updateProgress()) );
    connect(ui->stopRun, SIGNAL(clicked()), this, SLOT( modelHalt()) );

    dataTimer.start(0); // Interval 0 means to refresh as fast as possible
}

void MainWindow::modelUpdate(){

    unsigned int i, j, k, n, inc = 0;
    float theta_rad, a, b, c;
    float topFp, ovBank, ovFp;
    RiverProfile* rn = model->rn;
    hydro *wl = model->wl;
    sed *sd = model->sd;
    QVector<double> WSL( rn->nnodes );
    QVector<double> x( rn->nnodes );
    QVector<double> eta( rn->nnodes );
    QVector<double> bedrock( rn->nnodes );
    QVector<double> Froude( rn->nnodes );
    QVector<double> Bedload( rn->nnodes );
    QVector<double> Qw_Plot( rn->nnodes );
    QVector<double> LeftBankLower( rn->nnodes );
    QVector<double> RightBankLower( rn->nnodes );
    QVector<double> LeftBankTop( rn->nnodes );
    QVector<double> RightBankTop( rn->nnodes );
    QVector<double> XsPlotX( 11 );
    QVector<double> XsPlotY( 11 );
    QVector<double> wsXS_X( 2 );
    QVector<double> wsXS_Y( 2 );
    QVector<double> CursorX( 2 );
    QVector<double> CursorY( 2 );
    QVector<double> PSI( 8 );
    QVector<double> tmp( 8 );
    QVector<QVector<double> > GSD_Data, GSD_Cumul;

    tmp.fill( 0, rn -> nnodes );
    GSD_Data.fill( tmp, 7);
    GSD_Cumul.fill(tmp, 13);

    // Floating text items, in GSD graph
    QCPItemText *PSI4Label = new QCPItemText(ui->GSD_Dash);   // Label grain size info
    QCPItemText *PSI6Label = new QCPItemText(ui->GSD_Dash);   // Label grain size info
    QCPItemText *PSI8Label = new QCPItemText(ui->GSD_Dash);   // Label grain size info
    QCPItemText *PSI10Label = new QCPItemText(ui->GSD_Dash);   // Label grain size info

    // model iteration
    model->iteration();

    // dt control
    rn->dt = ui->deltaT->value();
    ui->dt_disp->setValue(rn->dt);                    // Control dt with slider
    rn->writeInterval = ui->writeInt_disp->value();

    // upwind control
    rn->sedUpw = ui->sedUpw_slider->value() / 100;
    rn->hydroUpw = ui->hydroUpw_slider->value() / 100;

    float timeleft = ( model->rn->endTime.getTime_t() - model->rn->dt ) - ( model->rn->cTime.getTime_t() );
    if ( timeleft <= 0 )
    {
        if ( ui->cycleButton->isChecked())
        {
            rn->cTime = wl->Qw[0][0].date_time;
        }
    else
        {
            dataTimer.stop();
            ui->runProgress->setValue(100);
            std::stringstream success_stream;
            success_stream << "Model Successfully Completed: Check Results File" << std::endl << std::endl;
            showInfoMessage("End Point Reached", success_stream);
            delete model;
        }
    }

    if (!ui->pausePlot->isChecked())
    {

        // Calculate water profile data points, bank widths
        for ( i = 0; i < rn -> nnodes; i++ )
        {
            x[i] = rn->RiverXS[i].node;
            eta[i] = rn->eta[i];
            bedrock[i] = rn->bedrock[i];
            WSL[i] = rn->eta[i] + rn->RiverXS[i].depth * 8;
            Froude[i] = rn->ntop[i];               // RiverXS[i].ustar;
            Bedload[i] = sd->Qs[i];

            LeftBankLower[i] = rn->RiverXS[i].width/2;
            RightBankLower[i] = -rn->RiverXS[i].width/2;
            theta_rad = rn->RiverXS[i].theta * PI / 180;
            LeftBankTop[i] = ( rn->RiverXS[i].width + (2 * ( rn->RiverXS[i].bankHeight - rn->RiverXS[i].Hmax) / tan( theta_rad ) ) ) / 2;
            RightBankTop[i] = -( rn->RiverXS[i].width + (2 * ( rn->RiverXS[i].bankHeight - rn->RiverXS[i].Hmax) / tan( theta_rad ) ) ) / 2;

            Qw_Plot[i] = wl->QwCumul[i] / 100;
            for ( j = 0; j < rn->ngsz; j++ )            // Make a cumulative dist
            {
                if ( j > 0 )
                    GSD_Cumul[j][i] = GSD_Cumul[j-1][i];
                for ( k = 0; k < rn->nlith; k++  )
                    GSD_Cumul[j][i] += rn->F[i].pct[k][j];
                if (GSD_Cumul[j][i] < 0.0001)
                    GSD_Cumul[j][i] = 0;
            }
        }

        // Setup cross-section graph
        n = ui->spinNode->value();
        theta_rad = rn->RiverXS[n].theta * PI / 180;
        a = rn->RiverXS[n].bankHeight - rn->RiverXS[n].Hmax;  // Vert & Horiz triangle segments at lower channel.
        c = tan(theta_rad);
        b = a / c;              // aka dW, horizontal distance between bed and bank, under toe of channel edge
        inc = 0.5;            // small increment to help with ordering linework.

        XsPlotX[2] = -1.5 * rn->RiverXS[n].fpSlope;
        XsPlotY[2] = 0;
        XsPlotX[3] = 0;
        XsPlotY[3] = -1.5;
        XsPlotX[4] = 0.001;
        XsPlotY[4] = -1.5 - rn->RiverXS[n].Hmax;
        XsPlotX[5] = b;
        XsPlotY[5] = -1.5 - rn->RiverXS[n].bankHeight;
        XsPlotX[6] = b + rn->RiverXS[n].width;
        XsPlotY[6] = XsPlotY[5];
        XsPlotX[7] = XsPlotX[6] + b;
        XsPlotY[7] = XsPlotY[4];
        XsPlotX[8] = XsPlotX[7] + 0.001;
        XsPlotY[8] = -1.5;
        XsPlotX[9] = XsPlotX[8] + 1.5;
        XsPlotY[9] = 0;
        XsPlotX[10] = XsPlotX[9] + 5 * rn->RiverXS[n].valleyWallSlp;
        XsPlotY[10] = 5;

        XsPlotX[1] = XsPlotX[2];
        XsPlotY[1] = 0;
        XsPlotX[0] = XsPlotX[1] - ( 5 * rn->RiverXS[n].valleyWallSlp );
        XsPlotY[0] = 5;

        wsXS_Y[0] = rn->RiverXS[n].depth;

        topFp = rn->RiverXS[n].bankHeight + 1.5;
        if (rn->RiverXS[n].depth > topFp)
        {
            ovFp = rn->RiverXS[n].depth - topFp;
            ovBank = 1.5;
            wsXS_X[0] = XsPlotX[1] - ( ovFp * rn->RiverXS[n].valleyWallSlp );
            wsXS_X[1] = XsPlotX[9] + ( ovFp * rn->RiverXS[n].valleyWallSlp );
            wsXS_Y[0] = ovFp;               // Floodplain elev. is '0' datum
            wsXS_Y[1] = wsXS_Y[0];
        }
        else if (rn->RiverXS[n].depth > rn->RiverXS[n].bankHeight)
        {
            ovBank = rn->RiverXS[n].depth - rn->RiverXS[n].bankHeight;
            wsXS_X[0] = - ( ovBank * rn->RiverXS[n].fpSlope );
            wsXS_X[1] = XsPlotX[8] + ovBank;
            wsXS_Y[0] = -1.5 + ovBank;
            wsXS_Y[1] = wsXS_Y[0];
        }
        else if (rn->RiverXS[n].depth > a)   // 'a' is computed, above, as bottom of vertical banks
        {
            wsXS_X[0] = 0;
            wsXS_X[1] = XsPlotX[8];
            wsXS_Y[0] = -1.5 - (rn->RiverXS[n].bankHeight - rn->RiverXS[n].depth);
            wsXS_Y[1] = wsXS_Y[0];
        }
        else   // otherwise, within the lower trapezoid
        {
            wsXS_X[0] = c;
            wsXS_X[1] = XsPlotX[8] - c;
            wsXS_Y[0] = -1.5 - (rn->RiverXS[n].bankHeight - rn->RiverXS[n].depth);
            wsXS_Y[1] = wsXS_Y[0];
        }

        // Grain size fining plot

        for ( j = 0; j < 7 ; j++ )
            for ( i = 0; i < rn -> nnodes; i++ )
                    GSD_Data[j][i] = GSD_Cumul[j*2][i];

        // t = ui->selectNode->value();

        // Longitudinal Plot

        CursorX[0] = ( wl->Qw[0][0].date_time.secsTo(rn->cTime) / 3600.);
        CursorX[1] = CursorX[0];
        CursorY[0] = 0;
        CursorY[1] = 9999;

        ui->VectorPlot->graph(2)->clearData();
        ui->VectorPlot->graph(2)->setData(x, eta);
        ui->VectorPlot->graph(3)->clearData();
        ui->VectorPlot->graph(3)->setData(x, WSL);
        //ui->VectorPlot->graph(1)->setPen(QPen(QColor(rand() % 254, rand() % 254, rand() % 254)));
        ui->VectorPlot->xAxis->setLabel("Distance Downstream");
        ui->VectorPlot->yAxis->setLabel("Elevation");
        ui->VectorPlot->xAxis->setRange(0,rn->nnodes + 1);
        ui->VectorPlot->yAxis->setRange(round(rn->eta[rn->nnodes-1]-10), round(rn->eta[1]+20));
        //ui->VectorPlot->setRangeDrag(Qt::Horizontal|Qt::Vertical);
        //ui->VectorPlot->setRangeZoom(Qt::Horizontal|Qt::Vertical);

        ui->BedloadPlot->graph(0)->clearData();
        ui->BedloadPlot->graph(0)->setData( x, Bedload );
        ui->BedloadPlot->graph(0)->setBrush(QColor(255, 161, 0, 50));
        ui->BedloadPlot->graph(0)->setChannelFillGraph(nullptr);
        ui->BedloadPlot->graph(1)->clearData();
        ui->BedloadPlot->graph(1)->setData( x, Qw_Plot );

        ui->BedloadPlot->xAxis->setLabel("Distance Downstream");
        ui->BedloadPlot->yAxis->setLabel("Qs, Qw Discharge");
        ui->BedloadPlot->xAxis->setRange(0, rn->nnodes + 1);
        ui->BedloadPlot->yAxis->setRange(0, Bedload[3] * 10 ); //round(*max_element(Bedload.constBegin(), Bedload.constEnd()) * 1.5));

        ui->BankWidthPlot->graph(0)->clearData();
        ui->BankWidthPlot->graph(0)->setData( x, LeftBankLower );
        ui->BankWidthPlot->graph(1)->clearData();
        ui->BankWidthPlot->graph(1)->setData( x, RightBankLower );
        ui->BankWidthPlot->graph(2)->clearData();
        ui->BankWidthPlot->graph(2)->setData( x, LeftBankTop );
        ui->BankWidthPlot->graph(3)->clearData();
        ui->BankWidthPlot->graph(3)->setData( x, RightBankTop );

        ui->BankWidthPlot->xAxis->setLabel("Distance Downstream");
        ui->BankWidthPlot->yAxis->setLabel("Bank Width");
        ui->BankWidthPlot->xAxis->setRange(0,rn->nnodes + 1);
        ui->BankWidthPlot->yAxis->setRange( ( *min_element(RightBankLower.constBegin(), RightBankLower.constEnd()) * 1.5),
                  ( round(*max_element(LeftBankLower.constBegin(), LeftBankLower.constEnd()) * 1.5 ) ) );

        ui->XSectPlot->xAxis->setRange( -20, 120 );
        ui->XSectPlot->yAxis->setRange( -7, 7 );
        ui->XSectPlot->graph(0)->clearData();
        ui->XSectPlot->graph(0)->setData( XsPlotX, XsPlotY );
        ui->XSectPlot->graph(1)->clearData();
        ui->XSectPlot->graph(1)->setData( wsXS_X, wsXS_Y );

        ui->QwSeries->graph(1)->clearData();
        ui->QwSeries->graph(1)->setData(CursorX, CursorY);

        // Grain Size info

        //2, 8, 32, 128mm fractions at position 0.2, 0.4, 0.6, 0.8
        if ((GSD_Data[2][rn->nnodes*0.2] > 0.1) && (GSD_Data[2][rn->nnodes*0.2] < 0.95))    // 2mm label
        {
            PSI4Label->setText( QString::number(pow(2,rn->F[0].psi[4])) + " mm");
            PSI4Label->position->setCoords(x[int(rn->nnodes*0.2)], GSD_Data[2][int(rn->nnodes*0.2)]);
            PSI4Label->setBrush(QColor(255, 255, 255, 127));
        }
        if ((GSD_Data[3][int(rn->nnodes*0.4)] > 0.1) && (GSD_Data[3][int(rn->nnodes*0.4)] < 0.9))    // 8mm label
        {
            PSI6Label->setText( QString::number(pow(2,rn->F[0].psi[6])) + " mm");
            PSI6Label->position->setCoords(x[int(rn->nnodes*0.4)], GSD_Data[3][int(rn->nnodes*0.4)]);
            PSI6Label->setBrush(QColor(255, 255, 255, 127));
        }
        if ((GSD_Data[4][int(rn->nnodes*0.6)] > 0.1) && (GSD_Data[4][int(rn->nnodes*0.6)] < 0.9))    // 32mm label
        {
            PSI8Label->setText( QString::number(pow(2,rn->F[0].psi[8])) + " mm");
            PSI8Label->position->setCoords(x[int(rn->nnodes*0.6)], GSD_Data[4][int(rn->nnodes*0.6)]);
            PSI8Label->setBrush(QColor(255, 255, 255, 127));
        }
        if ((GSD_Data[5][int(rn->nnodes*0.8)] > 0.1) && (GSD_Data[5][int(rn->nnodes*0.8)] < 0.9))    // 128mm label
        {
            PSI10Label->setText( QString::number(pow(2,rn->F[0].psi[10])) + " mm");
            PSI10Label->position->setCoords(x[int(rn->nnodes*0.8)], GSD_Data[5][int(rn->nnodes*0.8)]);
            PSI10Label->setBrush(QColor(255, 255, 255, 127));
        }

        //ui->GSD_Dash->graph(0)->clearData();
        ui->GSD_Dash->graph(0)->setData( x, GSD_Data[0] );
        ui->GSD_Dash->graph(1)->setData( x, GSD_Data[1] );
        ui->GSD_Dash->graph(2)->setData( x, GSD_Data[2] );
        ui->GSD_Dash->graph(3)->setData( x, GSD_Data[3] );
        ui->GSD_Dash->graph(4)->setData( x, GSD_Data[4] );
        ui->GSD_Dash->graph(5)->setData( x, GSD_Data[5] );
        ui->GSD_Dash->graph(6)->setData( x, GSD_Data[6] );
        ui->GSD_Dash->graph(0)->setPen(QPen(QColor(0,0,0)));  // 0 is the finest grain size category: black
        for ( i = 1; i < 7 ; i++ )
               ui->GSD_Dash->graph(i)->setPen(QPen(QColor((i * 30), 254 - (i * 30), 113)));

        ui->GSD_Dash->xAxis->setRange(0, rn->nnodes + 1);
        ui->GSD_Dash->yAxis->setRange(0, 1);

        ui->VectorPlot->replot();
        ui->BankWidthPlot->replot();
        ui->XSectPlot->replot();
        ui->BedloadPlot->replot();
        ui->QwSeries->replot();
        ui->GSD_Dash->replot();
        ui->grateDateTime->setDateTime(QDateTime::fromTime_t(rn->cTime.getTime_t()));

        ui->reportQw->setValue(wl->QwCumul[rn->nnodes-1] * rn->qwTweak);
        ui->reportQs->setValue(sd->Qs[0]);
        ui->reportStep->setValue(rn->counter);
        //ui->reportYear->setValue(rn->yearCounter);

        ui->spinBankHt->setValue(rn->RiverXS[n].bankHeight);
        ui->spinTheta->setValue(rn->RiverXS[n].theta);
        ui->spinDepth->setValue(rn->RiverXS[n].depth);
        ui->spinWidth->setValue(rn->RiverXS[n].width);
        ui->spinNoChnl->setValue(rn->RiverXS[n].noChannels);
        ui->spinD50->setValue(pow(2, rn->F[n].dsg));
        ui->spinDcomp->setValue(rn->RiverXS[n].comp_D);
        ui->spinD90->setValue(pow(2, rn->F[n].d90));
        ui->spinHmax->setValue(rn->RiverXS[n].Hmax);

    }  // End 'Pause Plot' statement

    // Update Regime switch

    rn->regimeFlag = ui->RegimeButton->isChecked();   // '1' means regime routine is on, else '0'
}

void MainWindow::modelHalt(){    // Graceful exit, here?
    dataTimer.stop();
    delete model;
}

void MainWindow::updateProgress(){

    RiverProfile* rn = model->rn;
    // Progress bar
    double i = rn->startTime.secsTo(rn->cTime);
    double j = rn->startTime.secsTo(rn->endTime);
    double prog = ( i / j ) * 100.;
    if (prog < 100.)
        ui->runProgress->setValue(prog);
}

MainWindow::~MainWindow()
{
    delete ui;
    if (initialised) {
        delete model;
    }
}
