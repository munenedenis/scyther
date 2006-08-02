#!/usr/bin/python

#---------------------------------------------------------------------------

""" Import externals """
import wx
import os.path
import sys
import wx.lib.mixins.listctrl as listmix

#---------------------------------------------------------------------------

""" Import scyther-gui components """
import Preference
import Attackwindow
import Scytherthread
import Icon

#---------------------------------------------------------------------------

""" Some constants """
ID_VERIFY = 100
ID_AUTOVERIFY = 101
ID_STATESPACE = 102
ID_CHECK = 103

#---------------------------------------------------------------------------

class MainWindow(wx.Frame):

    def __init__(self, filename=''):
        super(MainWindow, self).__init__(None, size=(600,800))
        self.dirname = '.'

        if filename != '' and os.path.isfile(filename):
            self.filename = filename
            self.load = True
        else:
            self.filename = 'noname.spdl'
            self.load = False

        Icon.ScytherIcon(self)

        self.CreateInteriorWindowComponents()
        self.CreateExteriorWindowComponents()

        aTable = wx.AcceleratorTable([
                                      #(wx.ACCEL_ALT,  ord('X'), exitID),
                                      (wx.ACCEL_CTRL, ord('W'), wx.ID_EXIT),
                                      (wx.ACCEL_NORMAL, wx.WXK_F1,
                                          ID_VERIFY),
                                      (wx.ACCEL_NORMAL, wx.WXK_F2,
                                          ID_STATESPACE),
                                      (wx.ACCEL_NORMAL, wx.WXK_F5, 
                                          ID_CHECK),
                                      (wx.ACCEL_NORMAL, wx.WXK_F6,
                                          ID_AUTOVERIFY),
                                      ])
        self.SetAcceleratorTable(aTable)

        self.claimlist = []
        self.pnglist = []

        #self.SetTitle(self.title) 

    def CreateInteriorWindowComponents(self):
        ''' Create "interior" window components. In this case it is just a
            simple multiline text control. '''

        self.splitter = wx.SplitterWindow(self,-1)
        self.splitter.daddy = self

        # Top: input
        self.top = wx.Notebook(self.splitter,-1)
        self.control = wx.TextCtrl(self.top, style=wx.TE_MULTILINE)
        if self.load:
            textfile = open(os.path.join(self.dirname, self.filename), 'r')
            self.control.SetValue(textfile.read())
            os.chdir(self.dirname)
            textfile.close()
        self.top.AddPage(self.control,"Protocol")
        self.settings = SettingsWindow(self.top,self)
        self.top.AddPage(self.settings,"Settings")

        # Bottom: output
        self.bottom = wx.Notebook(self.splitter,-1)
        self.report = SummaryWindow(self.bottom,self)
        self.bottom.AddPage(self.report,"Claim summary")
        self.errors = ErrorWindow(self.bottom,self)
        self.bottom.AddPage(self.errors,"Detailed output")

        #self.report.SetValue("Welcome.")

        # Combine
        self.splitter.SetMinimumPaneSize(20)
        self.splitter.SplitHorizontally(self.top, self.bottom)
        self.splitter.SetSashPosition(510)
        self.Show(1)


    def CreateExteriorWindowComponents(self):
        ''' Create "exterior" window components, such as menu and status
            bar. '''
        self.CreateMenus()
        self.CreateStatusBar()
        self.SetupToolBar()
        self.SetTitle()

    def SetupToolBar(self):

        tb = self.CreateToolBar(wx.TB_HORIZONTAL
                | wx.NO_BORDER
                | wx.TB_FLAT
                | wx.TB_TEXT
                )

        bmp = wx.ArtProvider_GetBitmap(wx.ART_EXECUTABLE_FILE,wx.ART_TOOLBAR,(20,20))
        if not bmp.Ok():
            bmp = wx.EmptyBitmap(20,20)
        tb.AddSimpleTool(ID_VERIFY, bmp,"Verify","Verify claims")
        self.Bind(wx.EVT_TOOL, self.OnVerify, id=ID_VERIFY)
        tb.AddSimpleTool(ID_STATESPACE, bmp,"Statespace","Generate statespace for all roles")
        self.Bind(wx.EVT_TOOL, self.OnStatespace, id=ID_STATESPACE)
        tb.AddSeparator()

        tb.AddSimpleTool(ID_CHECK, bmp,"Check","Check protocol")
        self.Bind(wx.EVT_TOOL, self.OnCheck, id=ID_CHECK)
        tb.AddSimpleTool(ID_AUTOVERIFY, bmp,"Default claims","Verify default claims")
        self.Bind(wx.EVT_TOOL, self.OnAutoVerify, id=ID_AUTOVERIFY)

    def CreateMenu(self, bar, name, list):

        fileMenu = wx.Menu()
        for id, label, helpText, handler in list:
            if id == None:
                fileMenu.AppendSeparator()
            else:
                item = fileMenu.Append(id, label, helpText)
                self.Bind(wx.EVT_MENU, handler, item)
        bar.Append(fileMenu, name) # Add the fileMenu to the MenuBar


    def CreateMenus(self):
        menuBar = wx.MenuBar()
        self.CreateMenu(menuBar, '&File', [
             (wx.ID_OPEN, '&Open', 'Open a new file', self.OnOpen),
             (wx.ID_SAVE, '&Save', 'Save the current file', self.OnSave),
             (wx.ID_SAVEAS, 'Save &As', 'Save the file under a different name',
                self.OnSaveAs),
             (None, None, None, None),
             (wx.ID_EXIT, 'E&xit\tCTRL-W', 'Terminate the program',
                 self.OnExit)])
        self.CreateMenu(menuBar, '&Verify',
             [(ID_VERIFY, '&Verify protocol\tF1','Verify the protocol in the buffer using Scyther',
                 self.OnVerify) ,
             (ID_STATESPACE, 'Generate &statespace\tF2','TODO' ,
                 self.OnAutoVerify) ,
             (None, None, None, None),
             (ID_CHECK, '&Check protocol\tF5','TODO',
                 self.OnStatespace) ,
             (ID_AUTOVERIFY, 'Verify &automatic claims\tF6','TODO',
                 self.OnCheck)
             ])
        self.CreateMenu(menuBar, '&Help',
            [(wx.ID_ABOUT, '&About', 'Information about this program',
                self.OnAbout) ])
        self.SetMenuBar(menuBar)  # Add the menuBar to the Frame


    def SetTitle(self):
        # MainWindow.SetTitle overrides wx.Frame.SetTitle, so we have to
        # call it using super:
        super(MainWindow, self).SetTitle('Scyther-gui: %s'%self.filename)


    # Helper methods:

    def defaultFileDialogOptions(self):
        ''' Return a dictionary with file dialog options that can be
            used in both the save file dialog as well as in the open
            file dialog. '''
        return dict(message='Choose a file', defaultDir=self.dirname,
                    wildcard='*.spdl')

    def askUserForFilename(self, **dialogOptions):
        dialog = wx.FileDialog(self, **dialogOptions)
        if dialog.ShowModal() == wx.ID_OK:
            userProvidedFilename = True
            self.filename = dialog.GetFilename()
            self.dirname = dialog.GetDirectory()
            self.SetTitle() # Update the window title with the new filename
        else:
            userProvidedFilename = False
        dialog.Destroy()
        return userProvidedFilename

    # Event handlers:

    def OnAbout(self, event):
        msg = "Scyther GUI\n\nScyther and Scyther GUI\ndeveloped by Cas Cremers"
        dialog = wx.MessageDialog(self,msg, 'About scyther-gui', wx.OK)
        dialog.ShowModal()
        dialog.Destroy()

    def OnExit(self, event):
        self.Close()  # Close the main window.

    def OnSave(self, event):
        textfile = open(os.path.join(self.dirname, self.filename), 'w')
        textfile.write(self.control.GetValue())
        textfile.close()

    def OnOpen(self, event):
        if self.askUserForFilename(style=wx.OPEN,
                                   **self.defaultFileDialogOptions()):
            textfile = open(os.path.join(self.dirname, self.filename), 'r')
            self.control.SetValue(textfile.read())
            textfile.close()

    def OnSaveAs(self, event):
        if self.askUserForFilename(defaultFile=self.filename, style=wx.SAVE,
                                   **self.defaultFileDialogOptions()):
            self.OnSave(event)
            os.chdir(self.dirname)

    def RunScyther(self, mode):
        Scytherthread.RunScyther(self,mode)

    def OnVerify(self, event):
        self.RunScyther("verify")

    def OnAutoVerify(self, event):
        self.RunScyther("autoverify")

    def OnStatespace(self, event):
        self.RunScyther("statespace")

    def OnCheck(self, event):
        self.RunScyther("check")

#---------------------------------------------------------------------------

class SettingsWindow(wx.Panel):

    def __init__(self,parent,daddy):
        wx.Panel.__init__(self,parent,-1)

        self.win = daddy


        # Bound on the number of runs
        self.maxruns = int(Preference.get('maxruns','5'))
        r1 = wx.StaticText(self,-1,"Maximum number of runs (0 disables bound)")
        l1 = wx.SpinCtrl(self, -1, "",size=(150,-1))
        l1.SetRange(0,100)
        l1.SetValue(self.maxruns)
        self.Bind(wx.EVT_SPINCTRL,self.EvtRuns,l1)

        # Matchin options
        self.match = int(Preference.get('match','0'))
        claimoptions = ['typed matching','find basic type flaws','find all type flaws']
        r2 = wx.StaticText(self,-1,"Matching type")
        l2 = wx.RadioBox(self, -1, "",
                wx.DefaultPosition,wx.DefaultSize,claimoptions,1,wx.RA_SPECIFY_COLS)
        l2.SetSelection(self.match)
        self.Bind(wx.EVT_RADIOBOX,self.EvtMatch,l2)

        ### MISC expert stuff

        self.misc = Preference.get('scytheroptions','')
        r10 = wx.StaticText(self,-1,"Additional parameters for the Scyther tool")
        l10 = wx.TextCtrl(self,-1,self.misc,size=(150,-1))
        self.Bind(wx.EVT_TEXT,self.EvtMisc,l10)

        # Combine
        space = 10
        sizer = wx.FlexGridSizer(cols=3, hgap=space,vgap=space)
        sizer.AddMany([ l1,r1, (0,0),
                        l2,r2, (0,0),
                        l10,r10, (0,0),
                        ])
        self.SetSizer(sizer)
        self.SetAutoLayout(True)

    def EvtMatch(self,evt):
        self.match = evt.GetInt()

    def EvtRuns(self,evt):
        self.maxruns = evt.GetInt()

    def EvtMisc(self,evt):
        self.misc = evt.GetString()

    def ScytherArguments(self):
        """ Note: constructed strings should have a space at the end to
            correctly separate the options.
        """

        tstr = ""

        # Number of runs
        tstr += "--max-runs=%s " % (str(self.maxruns))
        # Matching type
        tstr += "--match=%s " % (str(self.match))

        # Verification type
        if self.mode == "check":
            tstr += "--check "
        if self.mode == "autoverify":
            tstr += "--auto-claims "
        elif self.mode == "statespace":
            tstr += "--state-space "

        # Anything else?
        if self.misc != "":
            tstr += " " + self.misc + " "

        return tstr

#---------------------------------------------------------------------------

class SummaryWindow(wx.ListCtrl, listmix.ListCtrlAutoWidthMixin):

    def __init__(self,parent,daddy):
        wx.ListCtrl.__init__(self,parent,-1, style=wx.LC_REPORT
                | wx.BORDER_NONE
                | wx.LC_SORT_ASCENDING
                | wx.LC_SINGLE_SEL
                )
        listmix.ListCtrlAutoWidthMixin.__init__(self)

        self.win = daddy
        self.currentItem = -1

        self.Bind(wx.EVT_LIST_ITEM_SELECTED, self.OnItemSelected)

        self.InsertColumn(0, "Protocol")
        self.InsertColumn(1, "Role")
        self.InsertColumn(2, "Claim")
        self.InsertColumn(3, "Label")
        self.InsertColumn(4, "Parameters")
        self.InsertColumn(5, "Status")
        self.InsertColumn(6, "Attacks")
        self.InsertColumn(7, "Comments")

    def update(self):
        self.DeleteAllItems()
        self.claimlist = self.win.claimlist
        for key in range(0,len(self.claimlist)):
            cl = self.claimlist[key]
            index = self.InsertStringItem(sys.maxint,cl.protocol)
            self.SetStringItem(index,1,cl.role)
            self.SetStringItem(index,2,cl.claim)
            self.SetStringItem(index,3,cl.label)
            self.SetStringItem(index,4,cl.param)
            self.SetStringItem(index,5,cl.status)
            self.SetStringItem(index,6,str(cl.attackcount))
            self.SetStringItem(index,7,cl.comments)
            self.SetItemData(index,key)

            if cl.status == "Fail":
                # Failed :(
                item = self.GetItem(key)
                item.SetTextColour(wx.RED)
                self.SetItem(item)
            else:
                # Okay! But with bound?
                if cl.comments.find("bounds") == -1:
                    # No bounds, great :)
                    item = self.GetItem(key)
                    item.SetTextColour(wx.GREEN)
                    self.SetItem(item)
        #for i in range(0,7):
        #    self.SetColumnWidth(i,wx.LIST_AUTOSIZE)

    def OnItemSelected(self, event):
        self.currentItem = event.m_itemIndex
        cl = self.claimlist[self.currentItem]
        if cl.attackcount > 0:
            display = Attackwindow.AttackWindow(cl)
            display.Show(1)
        self.Refresh()
        event.Skip()

#---------------------------------------------------------------------------

class ErrorWindow(wx.TextCtrl):

    def __init__(self,parent,daddy):
        wx.TextCtrl.__init__(self,parent,-1, style=wx.TE_MULTILINE)
        self.win = daddy
    
    def update(self,summary):
        self.SetValue(summary)

#---------------------------------------------------------------------------

    
