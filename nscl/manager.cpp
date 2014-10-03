/***************************************************
# File Name:	manager.cpp
# Abstract:	
# Author:	zhangzhelucky
# Update History:
Wed May  8 14:17:58 2013 Take it from main.cpp
****************************************************/


#include "manager.h"
#include "log.h"

#include <iostream>
#include "stdio.h"
#include <stdlib.h>
#include <fstream>
#include <sstream>
#include <windows.h>

using namespace std;

CManager::CManager()
{
  system("cls");

  mVersion = "0.1.1";  // Initialize Software version number
  filename ="";
  PMTdir="";
  welcome();
  
  lock_isStarted = false;
  lock_isConfiged = true;
  lock_isDaqQuited = true;

  isPMT=false;
  isPMTConfiged=false;
  packet_num=100;
  current_limit = 50.0;
  warming_voltage = 500.0;
  pHVController=NULL;
  pPulser=NULL;

  m_hits = -1;
  pDisplay = NULL;
  pCCU = NULL;

  if( true == FirstLoad() )
    {
      pDisplay = new CDisplay();
      Config();
      //pthread_create( &mDisplayThread , NULL , displayThread , this );

      CmdAnalyse();
    }
}

CManager::~CManager()
{
  if( pCCU )
    delete pCCU;
  if( pDisplay )
    delete pDisplay;
  delModules();
  delModuleConfig();
}

void
CManager::delModules()
{
    int size=modules.size();
    if(size>0){
        for(int i=0;i<size;i++){
            delete modules[i];
        }
    }
    modules.clear();
}

void
CManager::delModuleConfig()
{
    int size=config_module.size();
    if(size>0){
        for(int i=0;i<size;i++){
            delete config_module[i];
        }
    }
    config_module.clear();
}

void CManager::CmdAnalyse()
{
  while(1)
    {
      int flag = pDisplay->getCmd();
      switch( flag )
        {
	case -1: // error commands ---------------------
	  {
          if(isPMT)
                pDisplay->output("Error Command.You're in PMT mode.");
          else
                pDisplay->output("Error Command.You're in Normal mode");
	    break;
	  }
	case 0:  // quit -------------------------------
	  {
	    pDisplay->output("Program is going to quit.");

        Sleep(1000);
	    CLog("Quit. ");
	    break;
	  }
	case 1:  // config -----------------------------
	  {
	    if ( true == lock_isStarted )
	      {
            pDisplay->output("Can't config while running. ");
	      }
	    else
	      {
            if ( true == Config() )
		  {
            pDisplay->output("Config Done. ");
		    lock_isConfiged = true;
		  }
            else
		  {
		    pDisplay->output("Config Failed. ");
		    return;
		  }
	      }
	    break;
	  }
    case 2:  // start daq cycle ---------------------
	  {
          if(isPMT){

          }
          else{
            if(filename.empty()){
                pDisplay->output("Please set the output filename first");
            }
            else{
                pDisplay->output("DAQ cycle is going to start.");

                if ( true == lock_isStarted )
                {
                    pDisplay->output("DAQ cycle is running now.Stop it first");
                }
                else
                {
                    lock_isStarted = true;
                    pDisplay->output("DAQ cycle is started.");
                    pDisplay->normal_status(false,filename.c_str(),"DAQ cycle is started.");

                    pthread_attr_t attr;
                    pthread_attr_init(&attr);
                    pthread_attr_setdetachstate( &attr ,
					     PTHREAD_CREATE_DETACHED);
                    pthread_create( &mDaqThread , NULL ,
                        daqThread , this );

                    m_Daq_start = clock();
                }
            }
          }
          break;
	  }
    case 3: // stop --------------------------------
	  {
          if(isPMT){

          }
          else{
            if ( lock_isStarted )
            {
                lock_isStarted = false;
                pDisplay->output("Waitting for DAQ quiting... ");
                //pDisplay->normal_status(false,filename.c_str(),"Waitting for DAQ quiting... ");
                while( false == lock_isDaqQuited );

                string tempstr;
                m_Daq_stop = clock();
                pDisplay->output("DAQ Stoped. ");
                tempstr="DAQ stopped successfully\n ";

                double time = (double)( m_Daq_stop - m_Daq_start )
                                        /(double)CLOCKS_PER_SEC;

                char buf1[100];
                pDisplay->output( "Statistics during last DAQ:" );
                sprintf( buf1 , "    time = %f s" , time);
                pDisplay->output( buf1 );
                tempstr.append(buf1);
                tempstr+="\n ";

                char buf2[100];
                sprintf( buf2 , "    hits = %d " , m_hits);
                pDisplay->output( buf2 );
                tempstr+=buf2;
                pDisplay->normal_status(true,NULL,tempstr.c_str());
            }
            else
            {
                pDisplay->output("No DAQ running. ");
            }
        }
	    break;
	  }
        case 4:
      {
          if(lock_isStarted){
              pDisplay->output("DAQ cycle is running now.Stop it first");
          }
          else{
              string tempstr;
              if(!filename.empty()){
                    stringstream ss;
                    ss<<"WARNING: previous filename\""<<filename<<"\""
                    <<" will be replaced";
                    pDisplay->output(ss.str().c_str());
                    tempstr=ss.str();
                    tempstr.append("\n ");
                }
                filename=pDisplay->getFilename();
                stringstream ss;
                ss<< "Data will be save to file: " << filename;
                pDisplay->output(ss.str().c_str());
                tempstr.append(ss.str());
                pDisplay->normal_status(true,filename.c_str(),tempstr.c_str());
          }
          break;
      }
      case 5:
      {
        if(lock_isStarted){
            pDisplay->output("DAQ cycle is running now.Stop it first");
        }
        else{
            CC_Config cur_config_cc;
            cur_config_cc=pCCU->getConfig();
            pDisplay->formCCU(cur_config_cc,config_module);
        }
        break;
      }
      case 6:
      {
          if(lock_isStarted){
              pDisplay->output("DAQ cycle is running now.Stop it first");
          }
          else{
              Module_Config cur_module_config;
              string tempstr;
              string module_name=pDisplay->getModulename();
              int size=modules.size();
              for(int i=0;i<size;i++){
                  tempstr=modules[i]->getName();
                  if(tempstr == module_name){
                      cur_module_config=modules[i]->getConfig();
                      pDisplay->formSingleModule(cur_module_config);
                      break;
                  }
              }
          }
          break;
      }
    case 7:
      {
          if(lock_isStarted){
              pDisplay->output("DAQ cycle is running now.Stop it first");
          }
          else{
              isPMT=true;
              pDisplay->output("you're in PMT testing mode now");
              pDisplay->pmt_status(true,0,0,NULL,"Initial State in PMT testing");
          }
          break;
	}
      case 8:
      {
        if(lock_isStarted){
            pDisplay->output("DAQ cycle is running now.Stop it first");
        }
        else{
            isPMT=false;
            pDisplay->output("you're in Normal testing mode now");
            pDisplay->normal_status(true,NULL,"Initial State in Normal testing");
        }
        break;
      }

      }
      flag = 100;
      
    }
}

bool CManager::daqCycle()
{
  //init
    daqInit();
  //start stack execute
    stackStart();
  //open file
  char buffer[4096*2];
  size_t buffersize=4096*2;//ccusb need buffersize larger than the actual buffer length
  size_t transferCount=0;
  FILE* fp=fopen(filename.c_str(),"wb");
  if(fp == NULL){
      stringstream tempstr;
      tempstr<<"can't open file: "<< filename;
      pDisplay->output(tempstr.str().c_str());
      return false;
  }
  int status;
  size_t writeCount;
  while(lock_isStarted){
       status=pCCU->usbRead(buffer,buffersize,&transferCount);
       if(status<0){
           pDisplay->output("waiting data...");
       }
       if(transferCount>0){
           writeCount=fwrite(buffer,sizeof(char),transferCount,fp);
           if(writeCount != transferCount){
               pDisplay->output("data written error, DAQ cycle terminated!");
               fclose(fp);
               stackStop();
               while(transferCount>0){
                   status=pCCU->usbRead(buffer,buffersize,&transferCount,3000);
               }
               daqClear();
               return false;
           }
       }
  }
  //stop stack execute
  stackStop();
  //read remaining packets
  transferCount=1;
  while(transferCount>0){
      status=pCCU->usbRead(buffer,buffersize,&transferCount,3000);
      if(transferCount>0){
          writeCount=fwrite(buffer,sizeof(char),transferCount,fp);
          if(writeCount != transferCount){
              pDisplay->output("CAUTION!: data written remaining error!");
          }
      }
  }
  //clean procedure
  daqClear();

  //close file
  fclose(fp);
  return true;
}

void CManager::daqInit()
{
    //select event trigger as scalor_a trigger
    //pCCU->writeDeviceSourceSelectors(0x4);
    //enable scalor_a
    pCCU->writeDeviceSourceSelectors(0x10);
    //clear scalor_a
    pCCU->writeDeviceSourceSelectors(0x20);

    //eable LAM
    int size=modules.size();
    for(int i=0;i<size;i++){
        modules[i]->clrDataMem();
        modules[i]->enableLam();
    }

    return;
}

void CManager::daqClear()
{
    //disable scalor_a
    pCCU->writeDeviceSourceSelectors(0x40);
    //clear scalor_a
    pCCU->writeDeviceSourceSelectors(0x20);
    //disable LAM
    int size=modules.size();
    for(int i=0;i<size;i++){
        modules[i]->disableLam();
        modules[i]->clrDataMem();
    }

    return;
}

void CManager::stackStart()
{
    pCCU->writeActionRegister(0x1);
}

void CManager::stackStop()
{
    pCCU->writeActionRegister(0x0);
}

bool CManager::FirstLoad()
{
  string timestr = CManager::getTimeStr();
  //string tempstr = "log/log_" + timestr;
  string tempstr="log.txt";
  CLog::fileChar = tempstr;

  CLog( timestr.c_str() ,true);

  cout << "Time: " << timestr << endl;
  cout << "Your working will be logged to file " 
       << CLog::fileChar << endl << endl;

  if( false == CcusbDevFind() )
    {
      return false;
    }
  CcusbDevOpen();
  
  return true;
}

bool CManager::Config()
{
  // Load config files
  bool flag = ConfigLoad();
  if( true == flag )
    {
      CLog("Load Config files successfully. ");
      pDisplay->output("Load Config files successfully. ");
    }
  else
    {
      CLog("Failed in Loading Config files. ");
      pDisplay->output("Failed in Loading Config files. ");
      return false;
    }

  // Config Devices =======================================
  // Config CCUsb --------------------------
  flag =  pCCU->config(config_cc);

  if( true == flag )
    {
      CLog("Set CCU successfully");
      pDisplay->output("Set CCU successfully. ");
    }
  else
    {
      CLog("Failed in setting CCU");
      pDisplay->output("Failed in setting CCU. ");
      return false;
    }

  // Config ADC ----------------------------
  pCCU->z();
  delModules();

  flag=true;
  NSCLmodule* tempmodule;
  int size=config_module.size();
  for(int i=0;i<size;i++){
      tempmodule=new NSCLmodule(pCCU);
      if(!tempmodule->config(*config_module[i])) {
          flag=false;
          break;
      }
      modules.push_back(tempmodule);
  }

  if( true == flag )
    {
      CLog("Set ADC successfully");
      pDisplay->output("Set ADC successfully. ");
    }
  else
    {
      CLog("Failed in Loading ADC Configs");
      pDisplay->output("Failed in setting ADC. ");
      return false;
    }

  // build stacklist and load
  buildStack();
  pCCU->loadList(0,stacklist);

  // Write the Configs of ADC to files ------------------------
  /*
  sParm para[16];
  pADC->getParaRegAll( para );
  string paraStr;

  paraStr = "Adc setting : \nChannel\tUT\tLT\tPED\n";
  CLog( paraStr.c_str() );

  for (int i = 0; i < 16; ++i)
    {
      stringstream stream;
      stream << i << "+"
	     << para[i].UT << "+"
	     << para[i].LT << "+"
	     << para[i].PED;
      stream >>paraStr;

      int pos = -1;
      while( 1 )
	{
	  pos = paraStr.find( '+', pos+1 );
	  if ( string::npos == pos)  break;
	  paraStr.replace( pos , 1 , 1 , '\t');
	}
      CLog( paraStr.c_str() );
    }
*/
  //display config
  CC_Config cur_config_cc=pCCU->getConfig();
  pDisplay->formCCU(cur_config_cc,config_module);

  return true;
}

void CManager::buildStack()
{
    stacklist.clear();
    //read Scalor_A
    stacklist.addRead24(25,11,0);
    //read DataMemory
    int size,n,a,f;
    uint16_t max=16;
    size=modules.size();
    for(int i=0;i<size;i++){
        //qscan
        n=modules[i]->getStation();a=0;f=0;
        stacklist.addQScan(n,a,f,max,true);
        //clear lam and data memory
        a=3;f=11;
        stacklist.addControl(n,a,f);
    }
    //add event terminator
    stacklist.addWrite16(0,0,16,0xEEEE);
}

void CManager::welcome()
{
  cout << "Welcome! "<<endl
       << "PMT Testing DAQ " << mVersion <<endl
       << "Copyright 2014. IMP. Zhou Yong. "<<endl
       << "This is free software with ABSOLUTELY NO WARRANTY. " 
       << endl << endl;
}

bool CManager::CcusbDevFind()
{
    int mDevNumber;
    vector<struct usb_device*> devices;
    devices=CCCUSB::enumerate();
    mDevNumber=devices.size();

    if ( 0==mDevNumber )
    {
      cout << "No CCUsb device found. " << endl
	   << "Please check whether your CCUsb devices is Powered. "
	   << endl << "Press [ Enter ] to quit. " << endl;
      getchar();
      return false;
    }
  else
    {
      cout << mDevNumber << " Device(s) Found. "
	   <<"Serial Number List:"<<endl;

      string tempstr;
      for (int i = 0; i < mDevNumber ; ++i)
	{
      tempstr=CCCUSB::serialNo(devices[i]);
      cout << "    Dev-"<< i <<"\t"<< tempstr << endl;
	}
    } 

  return true;
}

bool CManager::CcusbDevOpen()
{
  cout << "Press [Enter] to OPEN device listed... " << endl;
  getchar();
  pCCU = new CCCUSB();
  CLog("Device Open. ");

  return true;
}

bool CManager::CcuLoad()
{
    string tempstr;
    uint32_t tempint;
    // Load cc configs
    ifstream fp;
    fp.open( CC_ConfigPath );
    fp>>hex;
    if( !fp )
      {
        return false;
      }
    config_cc.clear();

    while( 1 )
      {
        fp >> tempstr;
        if( "CONFIG_BEGIN" == tempstr )
      {
        break;
      }
      }

    while( "CONFIG_END" != tempstr )
      {
        fp >> tempstr;

        if( tempstr == "GlobalMode" )
      {
        fp >> tempstr;
        fp >> tempint;
        config_cc.setGlobalMode(tempint);
      }
        else if( tempstr == "Delays" )
      {
        fp >> tempstr;
        fp >>tempint;
        config_cc.setDelays(tempint);
      }
        else if( tempstr == "ScalReadCtrl" )
      {
        fp >> tempstr;
        fp >>tempint;
        config_cc.setScalReadCtrl(tempint);
      }
        else if( tempstr == "SelectLED" )
      {
        fp >> tempstr;
        fp >> tempint;
        config_cc.setSelectLED(tempint);
      }
        else if( tempstr == "SelectNIMO" )
      {
        fp >> tempstr;
        fp >> tempint;
        config_cc.setSelectNIMO(tempint);
      }
        else if( tempstr == "SelectUserDevice" )
      {
        fp >> tempstr;
        fp >> tempint;
        config_cc.setSelectUserDevice(tempint);
      }
        else if( tempstr == "TimingDGGA" )
      {
        fp >> tempstr;
        fp >> tempint;
        config_cc.setTimingDGGA(tempint);
      }
        else if( tempstr == "TimingDGGB" )
      {
        fp >> tempstr;
        fp >> tempint;
        config_cc.setTimingDGGB(tempint);
      }
        else if( tempstr == "LAMMask" )
      {
        fp >> tempstr;
        fp >> tempint;
        config_cc.setLAMMask(tempint);
      }
        else if( tempstr == "ExtendedDelay" )
      {
        fp >> tempstr;
        fp >> tempint;
        config_cc.setExtendedDelay(tempint);
      }

        else if( tempstr == "UsbBufferSetup" )
      {
        fp >> tempstr;
        fp >> tempint;
        config_cc.setUsbBufferSetup(tempint);
      }
      }
    fp.close();

    return true;
}

bool CManager::ModuleLoad()
{
    // Load adc configs
    ifstream fp;
    string tempstr;
    int tempint_2;
    int counter=0;
    Module_Config* tempconfig;
    //
    fp.open( ADC_ConfigPath );
    if( !fp )
    {
        return false;
    }

    // Clear previous ModuleConfigFactory
    delModuleConfig();
    //
    while( 1 )
    {
        fp >> tempstr;
        if( "CONFIG_BEGIN" == tempstr )
        {
            break;
        }
    }
    while("CONFIG_END" != tempstr)
    {
        fp >> tempstr;
        if(tempstr == "STATION")
        {
            counter++;
            tempconfig=new Module_Config();
            //first is station number
            fp >> tempstr;
            fp >> tempint_2;
            tempconfig->setStation(tempint_2);

            while(tempstr != "STATION_END")
            {
                fp >> tempstr;
                if(tempstr == "NAME"){
                    fp >> tempstr;
                    fp >> tempstr;
                    tempconfig->setName(tempstr);
                }
                else if(tempstr == "Channel"){
                    //discard header
                    while( 1 )
                    {
                        fp >> tempstr;
                        if( "Index" == tempstr )
                        {
                            break;
                        }
                    }
                    //read config data
                    tempint_2=99;
                    while(tempint_2 >=0)
                    {
                        fp >> tempint_2;

                        if ( 0 == tempint_2 )
                        {
                            uint16_t ctrl[3];
                            for (int i = 0; i < 3; ++i)
                            {
                                fp >> ( ctrl[ i ] );
                            }
                            uint16_t control=(ctrl[0]<<2) + (ctrl[1]<<1) + ctrl[2];
                            tempconfig->setCtrl(control);
                        }
                        else if( tempint_2 > 0 && tempint_2 <= 16 )
                        {
                            int tmp[4];
                            for (int i = 0; i < 4 ; ++i)
                            {
                                fp >> tmp[i];
                            }
                            tempconfig->setUT(tempint_2,tmp[0]);
                            tempconfig->setLT(tempint_2,tmp[1]);
                            tempconfig->setPED(tempint_2,tmp[2]);
                        }
                    }
                }
            }
            config_module.push_back(tempconfig);
        }
    }

    if(config_module.size() != counter) return false;

    fp.close();

    return true;
}

bool CManager::CcudDefaultConfig()
{
    config_cc.clear();
    //Global: 4096 buffer length,extra buffer header
    config_cc.setGlobalMode(0x100);
    //Delay:  100us LAM timeout,8us trigger delay
    config_cc.setDelays(0x6408);
    //DeviceSource: event trigger as scalor_a source
    config_cc.setSelectUserDevice(0x4);

    return true;
}

bool CManager::ConfigLoad()
{
  //if(!CcuLoad())    return false;
  if(!CcudDefaultConfig())  return false;
  if(!ModuleLoad()) return false;

  return true;
}

string CManager::getTimeStr()
{
  time_t lt = time(NULL);
  tm* current = localtime( &lt );
  char str[100];
  strftime( str , 100 , "%y-%m-%d_%X", current);

  return std::string(str);

}

void* CManager::displayThread( void* args )
{
  CManager* pMan = ( CManager* )args;

  clock_t start, now;
  start = clock();


  while( 1 )
    {
      now = clock();
      //      if ( ( CLOCKS_PER_SEC ) < ( now - start ) )
      if ( 2000 < ( now - start ) )
	{
	  start = clock();
      //pMan->pDisplay->form( pMan->pADC , pMan->lock_isConfiged);

	  pMan->lock_isConfiged = false;
	}

    }

  return NULL;
}

void* CManager::daqThread( void* args )
{
  CManager* pMan = ( CManager* )args;

  pMan->lock_isDaqQuited = false;

  pMan->daqCycle();

  pMan->filename.clear();
  pMan->lock_isStarted = false;
  pMan->lock_isDaqQuited = true;

  return NULL;
}

//pmt testing
bool CManager::MkDir(const char* dir,char *msg)
{
    if (!CreateDirectory(dir, NULL))
    {
        if(ERROR_ALREADY_EXISTS == GetLastError()){
          sprintf(msg,"%s already exists",PMTdir.c_str());
          return false;
       }
       else if(ERROR_PATH_NOT_FOUND == GetLastError()){
          sprintf(msg,"%s path not found",PMTdir.c_str());
          return false;
       }
    }
    return true;
}

void CManager::delPMTConfig()
{
    if(isPMTConfiged){
        //
        size_t num=pHVGroup.size();
        for(size_t i=0;i<num;i++){
            delete pHVGroup[i];
        }
        pHVGroup.clear();
        //
        config_pmt.clear();
        config_hv.clear();
        //
        delete SYX527;
        delete AFG3252;
        //
        isPMTConfiged=false;
    }
}

bool CManager::ConfigPMT()
{
    //clean
    delPMTConfig();
    //reinit
    if(!_configAFG3252()){
        CLog("Failed in config AFG3252");
        return false;
    }
    if(!_configSYX527()){
        CLog("Failed in config SYX527");
        return false;
    }
    if(!_configTesting()){
        CLog("Failed in config PMT Tesing procedure");
        return false;
    }

    isPMTConfiged=true;
    CLog("Config PMT testing successfully");
    return true;
}

bool CManager::_configAFG3252()
{
    string IPAddr,name;
    string tempstr;
    ifstream fp;
    fp.open(PMTConfig_PATH);
    if(!fp){
        return false;
    }
    //
    while(1){
        fp>> tempstr;
        if(tempstr == "<AFG3252_BEGIN>"){
            break;
        }
    }

    while(tempstr != "</AFG3252_END>"){
        fp >> tempstr;
        if(tempstr == "IP"){
            fp >> tempstr;
            fp >> tempstr;
            IPAddr=tempstr;
        }
        else if(tempstr == "NAME"){
            fp >> tempstr;
            fp >> tempstr;
            name=tempstr;
        }
    }
    //
    pPulser=new AFG3252(name.c_str(),IPAddr.c_str());

    fp.close();
    return true;
}

bool CManager::_configSYX527()
{
    string IPAddr,UserName,PassWord;
    ushort slot,ch_id;
    string chname;
    HVChannel tempchannel;
    string tempstr;
    ifstream fp;
    fp.open(PMTConfig_PATH);
    if(!fp){
        return false;
    }
    //
    while(1){
        fp>> tempstr;
        if(tempstr == "<SYX527_BEGIN>"){
            break;
        }
    }

    while(tempstr != "</SYX527_END>"){
        fp >> tempstr;
        if(tempstr == "IP"){
            fp >> tempstr;
            fp >> tempstr;
            IPAddr=tempstr;
        }
        else if(tempstr == "USERNAME"){
            fp >> tempstr;
            fp >> tempstr;
            UserName=tempstr;
        }
        else if(tempstr == "PASSWORD"){
            fp >> tempstr;
            fp >> tempstr;
            PassWord=tempstr;
        }
        else if(tempstr == "<TABLE_BEGIN>"){
            fp >> tempstr;
            fp >> tempstr;
            fp >> tempstr;

            fp >> tempstr;
            while(tempstr != "</TABLE_END>"){
                //
                chname=tempstr;
                fp >> slot;
                fp >> ch_id;
                tempchannel.slot=slot;
                tempchannel.ch_id=ch_id;
                sprintf(tempchannel.ch_name,"%s",chname.c_str());
                config_hv[slot].push_back(tempchannel);
                //
                fp >> tempstr;
            }
        }
    }
    //
    pHVController=new SYX527(IPAddr.c_str(),UserName.c_str(),PassWord.c_str());
    SYX527_Module* tempmodule;
    HVGroup::iterator it;
    for(it=config_hv.begin();it!=config_hv.end();it++){
        tempmodule=new SYX527_Module(pHVController,it->first,it->second);
        tempmodule->updateChName();
        tempmodule->setRampUp(50.0);tempmodule->updateRampUp();
        tempmodule->setRampDown(50.0);tempmodule->updateRampDown();
        pHVGroup.push_back(tempmodule);
    }
    //
    fp.close();
    return true;
}

bool CManager::_configTesting()
{
    float voltage;
    LEDAmp templed;
    string tempstr;
    ifstream fp;
    fp.open(PMTConfig_PATH);
    if(!fp){
        return false;
    }
    //
    while(1){
        fp>> tempstr;
        if(tempstr == "<PMTTesting_BEGIN>"){
            break;
        }
    }

    while(tempstr != "</PMTTesting_END>"){
        fp >> tempstr;
        if(tempstr == "PACKET_NUMBER"){
            fp >> tempstr;
            fp >> packet_num;
        }
        else if(tempstr == "CURRENT_LIMIT"){
            fp >> tempstr;
            fp >> current_limit;
        }
        else if(tempstr == "WARMING_VOLTAGE"){
            fp >> tempstr;
            fp >> warming_voltage;
        }
        else if(tempstr == "<STEP_BEGIN>"){
            while(tempstr != "</STEP_END>"){
                fp >> tempstr;
                if(tempstr == "V0"){
                    fp >> tempstr;
                    fp >> voltage;
                }
                else if(tempstr == "<TABLE_BEGIN>"){
                    fp >> tempstr;
                    fp >> tempstr;
                    fp >> tempstr;
                    fp >> tempstr;
                    fp >> tempstr;
                    fp >> tempstr;

                    fp >> tempstr;
                    while(tempstr != "</TABLE_END>"){
                        fp >> templed.highV;
                        fp >> templed.frq;
                        fp >> templed.width;
                        fp >> templed.trigDelay;
                        fp >> templed.trigWidth;
                        config_pmt[voltage].push_back(templed);
                        //
                        fp >> tempstr;
                    }
                }
            }
        }
    }

    fp.close();
    return true;
}

void CManager::daqCycle(FILE* fp,unsigned long num)
{
    //init
    daqInit();
    //start stack execute
    stackStart();
    //open file
    char buffer[4096*2];
    size_t buffersize=4096*2;//ccusb need buffersize larger than the actual buffer length
    size_t transferCount=0;

    int status;
    unsigned long counter=0;
    while(counter<num){
         status=pCCU->usbRead(buffer,buffersize,&transferCount);
         if(status<0){
             pDisplay->output("waiting data...");
         }
         if(transferCount>0){
             counter++;
             fwrite(buffer,sizeof(char),transferCount,fp);
             if((counter%100) == 0){
                 char msg[100];
                 sprintf(msg,"%d packets",i);
                 pDisplay->output(msg);
             }
         }
    }
    //stop stack execute
    stackStop();
    //read remaining packets
    transferCount=1;
    while(transferCount>0){
        status=pCCU->usbRead(buffer,buffersize,&transferCount,3000);
    }
    //clean procedure
    daqClear();

    return;
}

void CManager::pmtTesting()
{
    double warming_seconds=60*10;//warming time
    double balance_seconds=60*2;//HV stablizing time
    double interval;
    time_t begin,end;
    ofstream fp_config;
    ofstream fp_log;
    ofstream fp_ledconfig;
    FILE *fp_raw;

    string tempstr;
    stringstream tempss;
    tempss<<setprecision(2);
    //mk raw dir
    char msg[256];
    string hv_dir,raw_filename;
    string ledconfig_filename;
    string raw_dir=PMTdir+"/raw_data";
    if(!MkDir(raw_dir.c_str(),msg)){
        pDisplay->output(msg);
        return;
    }

    //open log file and pmt config file
    string log_filename=raw_dir+"/log.txt";
    fp_log.open(log_filename.c_str());
    if(!fp_log){
        pDisplay->output("can't open "+log_filename);
        return;
    }
    fp_log << "Logging info of this PMT testing" <<endl;

    string config_filename=raw_dir+"/pmt.conf";
    fp_config.open(config_filename.c_str());
    if(!fp_config){
        pDisplay->output("can't open "+config_filename);
        return;
    }

    //power on sy1527 and pmt warming
    _setV(warming_voltage);
    _setI(current_limit);
    _powerOn();
    tempstr=getTimeStr() + ": Power On SY1527";
    pDisplay->output(tempstr);
    fp_log << tempstr <<endl;
    tempstr=getTimeStr() + ": PMT warming started.";
    pDisplay->output(tempstr);
    fp_log << getTimeStr() <<": PMT warming started.Warming Voltage is "<<warming_voltage<<"V"<<endl;

    begin=time(NULL);
    end=time(NULL);
    interval=difftime(end,begin);
    while(interval < warming_seconds){
        Sleep(60000);
        end=time(NULL);
        interval=difftime(end,begin);
    }

    tempstr=getTimeStr() + ": PMT warming stopped.";
    pDisplay->output(tempstr);
    fp_log << tempstr <<endl;

    _HVfeedback();
    fp_log<< _formatHVGroup();

    //LED pulser init
    _PulserInit();

    //pedestal testing before LED sweep
    tempstr=raw_dir+"/pedestal";
    if(!MkDir(tempstr.c_str(),msg)){
        pDisplay->output(msg);
        return;
    }

    raw_filename=raw_dir+"/pedestal/before.dat";
    fp_raw=fopen(raw_filename.c_str(),"wb");
    if(!fp_raw){
        pDisplay->output("can't open "+tempstr);
        return;
    }
    tempstr=getTimeStr()+": Pedestal Testing before formal testing";
    fp_log<<tempstr<<endl;
    pDisplay->output(tempstr);

    daqCycle(fp_raw,packet_num);
    fclose(fp_raw);

    tempstr=getTimeStr()+": Pedestal Testing completed";
    fp_log<<tempstr<<endl;
    pDisplay->output(tempstr);

    //formal testing
    fp_log<<"\nFormal Testing"<<endl;
    size_t voltage_step=config_pmt.size();
    fp_config << "Voltage Step: "<<voltage_step<<endl;
    size_t led_step;
    int tempvolt;
    PMTTestingConfig::iterator it;
    LEDPulserConfig tempLEDConfig;

    for(it=config_pmt.begin();it!=config_pmt.end();it++){
        //set voltage
        _setV(it->first);
        tempvolt=static_cast<int>(it->first);
        fp_config<< tempvolt <<"V"<<endl;
        //mkdir
        tempss.clear();tempss.str("");
        tempss<<raw_dir<<"/"<<tempvolt<<"V";
        hv_dir=tempss.str();
        if(!MkDir(hv_dir.c_str(),msg)){
            pDisplay->output(msg);
            return;
        }

        //stablizing
        begin=time(NULL);
        end=time(NULL);
        interval=difftime(end,begin);
        while(interval < balance_seconds){
            Sleep(60000);
            end=time(NULL);
            interval=difftime(end,begin);
        }
        //logging
        tempss.clear();tempss.str("");
        tempss<<getTimeStr()<<": HV is "<<it->first<<"V.Testing begin..."<<endl;
        tempstr=tempss.str();
        fp_log<<tempstr;
        pDisplay->output(tempstr);

        //testing begin
        tempLEDConfig=it->second;
        led_step=tempLEDConfig.size();

        for(size_t i=0;i<led_step;i++){

        }
    }
    //pedestal testing after LED sweep
}

void CManager::_setV(float voltage)
{
    size_t size=pHVGroup.size();
    for(size_t i=0;i<size;i++){
        pHVGroup[i]->setVSet(voltage);
        pHVGroup[i]->updateVSet();
    }
}

void CManager::_setI(float current)
{
    size_t size=pHVGroup.size();
    for(size_t i=0;i<size;i++){
        pHVGroup[i]->setISet(current);
        pHVGroup[i]->updateISet();
    }
}

void CManager::_setRup(float rup)
{
    size_t size=pHVGroup.size();
    for(size_t i=0;i<size;i++){
        pHVGroup[i]->setRampUp(rup);
        pHVGroup[i]->updateRampUp();
    }
}

void CManager::_setRDwn(float rdwn)
{
    size_t size=pHVGroup.size();
    for(size_t i=0;i<size;i++){
        pHVGroup[i]->setRampDown(rdwn);
        pHVGroup[i]->updateRampDown();
    }
}

void CManager::_powerOn()
{
    size_t size=pHVGroup.size();
    for(size_t i=0;i<size;i++){
        pHVGroup[i]->PowerOn();
    }
}

void CManager::_powerOff()
{
    size_t size=pHVGroup.size();
    for(size_t i=0;i<size;i++){
        pHVGroup[i]->PowerOff();
    }
}

void CManager::_HVfeedback()
{
    size_t size=pHVGroup.size();
    for(size_t i=0;i<size;i++){
        pHVGroup[i]->update(config_hv[pHVGroup[i].getSlot()]);
    }
}

string CManager::_formatHVGroup()
{
    HVGroup::iterator it;
    HVChannels tempChs;
    size_t channels_size;
    stringstream ss;
    ss<<"\t\tCurrent Monitoring Value:\n";

    ss<<"\t\tCH:";
    for(it=config_hv.begin();it!=config_hv.end();it++){
        tempChs=it->second;
        channels_size=tempChs.size();
        for(size_t j=0;j<channels_size;j++){
            ss<<"\t"<<tempChs[j].slot<<"_"<<tempChs[j].ch_id;
        }
    }
    ss<<"\n";

    ss<<setprecision(2);
    ss<<"\t\tVMon:";
    for(it=config_hv.begin();it!=config_hv.end();it++){
        tempChs=it->second;
        channels_size=tempChs.size();
        for(size_t j=0;j<channels_size;j++){
            ss<<"\t"<<tempChs[j].VMon;
        }
    }
    ss<<"\n";

    ss<<"\t\tIMon:";
    for(it=config_hv.begin();it!=config_hv.end();it++){
        tempChs=it->second;
        channels_size=tempChs.size();
        for(size_t j=0;j<channels_size;j++){
            ss<<"\t"<<tempChs[j].IMon;
        }
    }
    ss<<"\n";

    return ss.str();
}

void CManager::_PulserInit()
{
    //Reset
    pPulser->Reset();
    pPulser->PowerOff(1);pPulser->PowerOff(2);
    //set shape
    pPulser->SetShape(1);pPulser->SetShape(2);
    //sychronize
    pPulser->TurnOnFrqConcurrent();
    pPulser->PhaseInitiate();
    //output2 is gate NIM
    pPulser->SetPolarity(2,false);
    pPulser->SetFrequency(1,1000);
    pPulser->SetVoltageHigh(2,0);
    pPulser->SetVoltageLow(2,-0.8);
    pPulser->SetPulseWidth(2,200);
    //output1 is positive
    pPulser->SetVoltageLow(1,0);
}
