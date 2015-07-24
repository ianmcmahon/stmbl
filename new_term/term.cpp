#include <wx/wxprec.h>

#ifndef WX_PRECOMP
#include <wx/wx.h>
#endif

#include "mathplot.h"

#include <wx/image.h>
#include <wx/listctrl.h>
#include <wx/sizer.h>
#include <wx/log.h>
#include <wx/intl.h>
#include <wx/print.h>

#include <math.h>
// #include <time.h>

class mpScopeWave : public mpFX{
private:
   enum{
      FLOATING,
      ROLLING
   } mode;

   std::vector<double> m_ys;
   double m_minY, m_maxY;

   float sampleFreq;

   float sampleTime;

   int buffer_pos; // current write pos in data buffer
   int start_pos; // current start pos in data buffer

   unsigned int min_index;
   unsigned int max_index;


   void updateMinMaxY(){
      for(int i = 0; i < m_ys.size(); i++){
         if(m_ys[i] < m_minY){
            m_minY = m_ys[i];
            min_index = i;
         }
         if(m_ys[i] > m_maxY){
            m_maxY = m_ys[i];
            max_index = i;
         }
      }
   }

   // void createTimeVector(){
   //    m_xs.clear();
   //
   //    for(int i = 0; i < l; i++){
   //       m_xs.push_back(-sampleTime + i * sampleTime / l);
   //    }
   //    m_minX = -sampleTime;
   //    m_maxX = 0.0;
   //
   //    if(m_ys.size() > m_xs.size()){
   //       min_index -= m_ys.size() - m_xs.size();
   //       max_index -= m_ys.size() - m_xs.size();
   //       m_ys.erase(m_ys.begin(), m_ys.begin() + (m_ys.size() - m_xs.size()));
   //
   //       if(min_index < 0 || max_index < 0){
   //          updateMinMaxY();
   //       }
   //    }
   //    else{
   //       int j = m_xs.size() - m_ys.size();
   //       for(int i = 0; i < j; i++){
   //          m_ys.push_back((m_maxY + m_minY) / 2.0);
   //       }
   //    }
   // }

public:
   mpScopeWave(wxString name = wxEmptyString, int flags = mpALIGN_NE) : mpFX(name, flags){
      sampleFreq = 1.0;
      sampleTime = 1.0;
      mode = FLOATING;
      buffer_pos = 0;
      start_pos = 0;
      m_ys.push_back(0.0);
      //createTimeVector();
   }

   double GetY(double  x){
      if(x >= -sampleTime && x <= 0.0){
         return(m_ys[(int)((x + sampleTime) * sampleFreq + start_pos) % m_ys.size()]);
      }
      else{
         return(0.0);
      }
   }

   double GetMinX(){
      return(-sampleTime);
   }

   double GetMaxX(){
      return(0.0);
   }

   void AddData(float y){
      buffer_pos++;
      buffer_pos %= m_ys.size();

      if(mode == FLOATING){
         start_pos = buffer_pos;
      }

      m_ys[buffer_pos] = y;

      if(buffer_pos == min_index || buffer_pos == max_index){
         updateMinMaxY();
      }
      else{
         if(y < m_minY){
            m_minY = y;
            min_index = buffer_pos;
         }
         if(y > m_maxY){
            m_maxY = y;
            max_index = buffer_pos;
         }
      }
   }

   void setFloating(){
      mode = FLOATING;
   }
   void setRolling(){
      mode = ROLLING;
   }

   void setSampleFreq(float f){
      sampleFreq = fmax(f, 0.0);
      int l = fmax(sampleTime * sampleFreq, 1.0);
      std::cout << "buffer size: " << m_ys.size() << " -> " << l << std::endl;
      if(m_ys.size() > l){
         min_index -= m_ys.size() - l;
         max_index -= m_ys.size() - l;
         m_ys.erase(m_ys.begin(), m_ys.begin() + (m_ys.size() - l));

         buffer_pos = fmin(buffer_pos, m_ys.size());
         start_pos = fmin(start_pos, m_ys.size());

         if(min_index < 0 || max_index < 0){
            updateMinMaxY();
         }
      }
      else if(m_ys.size() < l){
         int j = l - m_ys.size();
         for(int i = 0; i < j; i++){
            m_ys.push_back((m_maxY + m_minY) / 2.0);
         }
      }
   }

   void setSampleTime(float t){
      sampleTime = fmax(t, 0.0);
      int l = fmax(sampleTime * sampleFreq, 1.0);
      std::cout << "buffer size: " << m_ys.size() << " -> " << l << std::endl;
      if(m_ys.size() > l){
         min_index -= m_ys.size() - l;
         max_index -= m_ys.size() - l;
         m_ys.erase(m_ys.begin(), m_ys.begin() + (m_ys.size() - l));

         buffer_pos = fmin(buffer_pos, m_ys.size());
         start_pos = fmin(start_pos, m_ys.size());

         if(min_index < 0 || max_index < 0){
            updateMinMaxY();
         }
      }
      else if(m_ys.size() < l){
         int j = l - m_ys.size();
         for(int i = 0; i < j; i++){
            m_ys.push_back((m_maxY + m_minY) / 2.0);
         }
      }
   }
};

class MyFrame;
class MyApp;

class MyFrame: public wxFrame
{
public:
   MyFrame();

   void OnAbout( wxCommandEvent &event );
   void OnQuit( wxCommandEvent &event );
   void OnFit( wxCommandEvent &event );
   void OnTimer(wxTimerEvent& event);
   void OnRefresh(wxCommandEvent& event);
   void OnConnect(wxCommandEvent& event);
   void OnDisconnect(wxCommandEvent& event);

   mpWindow        *m_plot;
   wxTextCtrl      *m_log;
   wxTextCtrl      *m_cli;
   wxTimer         *m_Timer;

private:
   int axesPos[2];
   bool ticks;
   mpScopeWave *wave0;

   DECLARE_DYNAMIC_CLASS(MyFrame)
   DECLARE_EVENT_TABLE()
};

// MyApp

class MyApp: public wxApp
{
public:
   virtual bool OnInit();
};

// main program

IMPLEMENT_APP(MyApp)

// MyFrame

enum {
   ID_QUIT  = 108,
   ID_ABOUT,
   ID_PRINT,
   ID_PRINT_PREVIEW,
   ID_ALIGN_X_AXIS,
   ID_ALIGN_Y_AXIS,
   ID_TOGGLE_GRID,
   TIMER_ID
};

IMPLEMENT_DYNAMIC_CLASS( MyFrame, wxFrame )

BEGIN_EVENT_TABLE(MyFrame,wxFrame)
EVT_MENU(ID_ABOUT, MyFrame::OnAbout)
EVT_MENU(ID_QUIT,  MyFrame::OnQuit)
EVT_MENU(mpID_FIT, MyFrame::OnFit)
// EVT_MENU(mpID_Refresh, MyFrame::OnRefresh)
// EVT_MENU(mpID_Connect, MyFrame::OnConnect)
// EVT_MENU(mpID_Disconnect, MyFrame::OnDisconnect)
EVT_TIMER(TIMER_ID, MyFrame::OnTimer)
END_EVENT_TABLE()

MyFrame::MyFrame()
: wxFrame( (wxFrame *)NULL, -1, wxT("wxWindows mathplot sample #3 - 'mpMovableObject' objects"),
wxDefaultPosition, wxSize(500,500) )
{
   wxMenu *file_menu = new wxMenu();
   wxMenu *view_menu = new wxMenu();

   file_menu->Append( ID_ABOUT, wxT("&About..."));
   file_menu->Append( ID_QUIT,  wxT("E&xit\tAlt-X"));
   //  file_menu->Append( mpID_Refresh,  wxT("Refresh\tCtrl-R"));
   //  file_menu->Append( mpID_Connect,  wxT("Connect\tCtrl-C"));
   //  file_menu->Append( mpID_Disconnect,  wxT("Disconnect\tCtrl-D"));

   view_menu->Append( mpID_FIT,      wxT("&Fit bounding box"), wxT("Set plot view to show all items"));
   view_menu->Append( mpID_ZOOM_IN,  wxT("Zoom in"),           wxT("Zoom in plot view."));
   view_menu->Append( mpID_ZOOM_OUT, wxT("Zoom out"),          wxT("Zoom out plot view."));

   wxMenuBar *menu_bar = new wxMenuBar();
   menu_bar->Append(file_menu, wxT("&File"));
   menu_bar->Append(view_menu, wxT("&View"));

   SetMenuBar( menu_bar );
   CreateStatusBar(1);

   //mpLayer* l;

   m_plot = new mpWindow( this, -1, wxPoint(0,0), wxSize(100,100), wxSUNKEN_BORDER );
   m_plot->SetMargins(0,0,40,40);
   mpScaleX* xaxis = new mpScaleX(wxT("t"), mpALIGN_BOTTOM, true);
   mpScaleY* yaxis = new mpScaleY(wxT("y"), mpALIGN_LEFT, true);
   xaxis->SetDrawOutsideMargins(false);
   yaxis->SetDrawOutsideMargins(false);
   m_plot->AddLayer(xaxis);
   m_plot->AddLayer(yaxis);

   wave0 = new mpScopeWave(wxT("wave0"));
   wave0->SetPen( wxPen(*wxBLACK, 1, wxSOLID));
   wave0->setSampleTime(10.0);
   wave0->setRolling();
   wave0->SetContinuity(true);

   m_plot->AddLayer(wave0);

   m_log = new wxTextCtrl( this, -1, wxT(""), wxPoint(0,0), wxSize(100,100), wxTE_MULTILINE );
   m_cli = new wxTextCtrl( this, -1, wxT(""), wxPoint(0,0), wxSize(100,100), wxTE_PROCESS_ENTER);
   //wxLog *old_log = wxLog::SetActiveTarget( new wxLogTextCtrl( m_log ) );
   //delete old_log;

   //wxStreamToTextRedirector redirect(m_log);

   wxBoxSizer *topsizer = new wxBoxSizer( wxVERTICAL );

   topsizer->Add( m_plot, 1, wxEXPAND );
   topsizer->Add( m_log, 0, wxEXPAND );
   topsizer->Add( m_cli, 0, wxEXPAND );

   SetAutoLayout( TRUE );
   SetSizer( topsizer );
   axesPos[0] = 0;
   axesPos[1] = 0;
   ticks = true;


   m_plot->EnableDoubleBuffer(true);
   m_plot->LockAspect( false );
   m_plot->Fit();

   m_Timer = new wxTimer(this,TIMER_ID);
   m_Timer->Start( 20 );
   wave0->setSampleFreq(100/0.02);
   wave0->setSampleFreq(100/0.02);
}

void MyFrame::OnQuit( wxCommandEvent &WXUNUSED(event) )
{
   Close( TRUE );
}

void MyFrame::OnFit( wxCommandEvent &WXUNUSED(event) )
{
   m_plot->Fit();
}

void MyFrame::OnAbout( wxCommandEvent &WXUNUSED(event) )
{
   wxMessageBox( wxT("STMBL Term"));
}

void MyFrame::OnTimer(wxTimerEvent& event)
{
   static float time = 0.0;
   for(int i = 0; i < 100; i++){
      wave0->AddData(sin(time));
      time += m_Timer->GetInterval()/1000.0/100.0*1.0;
   }
   m_plot->UpdateAll();
}

//-----------------------------------------------------------------------------
// MyApp
//-----------------------------------------------------------------------------

bool MyApp::OnInit()
{
   wxFrame *frame = new MyFrame();
   frame->Show( TRUE );

   return TRUE;
}
