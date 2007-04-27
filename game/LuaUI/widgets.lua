--------------------------------------------------------------------------------
--------------------------------------------------------------------------------
--
--  file:    widgets.lua
--  brief:   the widget manager, a call-in router
--  author:  Dave Rodgers
--
--  Copyright (C) 2007.
--  Licensed under the terms of the GNU GPL, v2 or later.
--
--------------------------------------------------------------------------------
--------------------------------------------------------------------------------

function pwl() -- ???  (print widget list)
  for k,v in ipairs(widgetHandler.widgets) do
    print(k, v.whInfo.layer, v.whInfo.name)
  end
end


include("keysym.h.lua")
include("utils.lua")
include("system.lua")
include("savetable.lua")


local gl = Spring.Draw

local ORDER_FILENAME     = LUAUI_DIRNAME .. 'Config/widget_order.lua'
local CONFIG_FILENAME    = LUAUI_DIRNAME .. 'Config/widget_data.lua'
local WIDGET_DIRNAME     = LUAUI_DIRNAME .. 'Widgets/'
local MOD_WIDGET_DIRNAME = MODUI_DIRNAME .. 'Widgets/'

local SELECTOR_BASENAME = 'selector.lua'


local SAFEWRAP = 1
-- 0: disabled
-- 1: enabled, but can be overriden by widget.GetInfo().unsafe
-- 2: always enabled


--------------------------------------------------------------------------------

-- install bindings for TweakMode and the Widget Selector

Spring.SendCommands({
  "unbindkeyset  Any+f11",
  "unbindkeyset Ctrl+f11",
  "bind    f11  luaui selector",
  "bind  C+f11  luaui tweakgui",
  "echo LuaUI: bound F11 to the widget selector",
  "echo LuaUI: bound CTRL+F11 to tweak mode"
})


--------------------------------------------------------------------------------
--------------------------------------------------------------------------------
--
--  the widgetHandler object
--

widgetHandler = {

  widgets = {},

  configData = {},
  orderList = {},

  knownWidgets = {},
  knownCount = 0,
  knownChanged = true,

  commands = {},
  customCommands = {},
  inCommandsChanged = false,

  actionHandler = include("actions.lua"),
  
  WG = {}, -- shared table for widgets

  mouseOwner = nil,
  ownedButton = 0,
  
  tweakMode = false,

  xViewSize    = 1,
  yViewSize    = 1,
  xViewSizeOld = 1,
  yViewSizeOld = 1,
}


-- these call-ins are set to 'nil' if not used
-- they are setup in UpdateCallIns()
local flexCallIns = {
  'GameOver',
  'TeamDied',
  'UnitCreated',
  'UnitFinished',
  'UnitFromFactory',
  'UnitDestroyed',
  'UnitIdle',
  'UnitTaken',
  'UnitGiven',
  'UnitEnteredRadar',
  'UnitEnteredLos',
  'UnitLeftRadar',
  'UnitLeftLos',
  'UnitSeismicPing',
  'DrawWorldShadow',
  'DrawWorldReflection',
  'DrawWorldRefraction',
  'DrawInMiniMap'
}

local callInLists = {
  'Shutdown',
  'Update',
  'TextCommand',
  'CommandNotify',
  'AddConsoleLine',
  'ViewResize',
  'DrawWorld',
  'DrawScreen',
  'KeyPress',
  'KeyRelease',
  'MousePress',
  'IsAbove',
  'GetTooltip',
  'GroupChanged',
  'CommandsChanged',
  'TweakMousePress',
  'TweakIsAbove',
  'TweakGetTooltip',

-- these use mouseOwner instead of lists
--  'MouseMove',
--  'MouseRelease',
--  'TweakKeyPress',
--  'TweakKeyRelease',
--  'TweakMouseMove',
--  'TweakMouseRelease',

-- uses the DrawScreenList
--  'TweakDrawScreen',
}

-- append the flex call-ins
for _,uci in ipairs(flexCallIns) do
  table.insert(callInLists, uci)
end


-- initialize the call-in lists
do
  for _,listname in ipairs(callInLists) do
    widgetHandler[listname..'List'] = {}
  end
end


--------------------------------------------------------------------------------
--------------------------------------------------------------------------------
--
--  Reverse integer iterator for drawing
--

local function rev_iter(t, key)
  if (key <= 1) then
    return nil
  else
    local nkey = key - 1
    return nkey, t[nkey]
  end
end

local function ripairs(t)
  return rev_iter, t, (1 + table.getn(t))
end


--------------------------------------------------------------------------------
--------------------------------------------------------------------------------

function widgetHandler:LoadOrderList()
  local chunk, err = loadfile(ORDER_FILENAME)
  if (chunk == nil) then
    return {}
  else
    local tmp = {}
    setfenv(chunk, tmp)
    self.orderList = chunk()
    if (not self.orderList) then
      self.orderList = {} -- safety
    end
  end
end


function widgetHandler:SaveOrderList()
  -- update the current order
  for i,w in (self.widgets) do
    self.orderList[w.whInfo.name] = i
  end
  table.save(self.orderList, ORDER_FILENAME,
             '-- Widget Order List  (0 disables a widget)')
end


--------------------------------------------------------------------------------

function widgetHandler:LoadConfigData()
  local chunk, err = loadfile(CONFIG_FILENAME)
  if (chunk == nil) then
    return {}
  else
    local tmp = {}
    setfenv(chunk, tmp)
    self.configData = chunk()
    if (not self.configData) then
      self.configData = {} -- safety
    end
  end
end


function widgetHandler:SaveConfigData()
  self:LoadConfigData()
  for _,w in ipairs(self.widgets) do
    if (w.GetConfigData) then
      self.configData[w.whInfo.name] = w:GetConfigData()
    end
  end
  table.save(self.configData, CONFIG_FILENAME, '-- Widget Custom Data')
end


function widgetHandler:SendConfigData()
  self:LoadConfigData()
  for _,w in ipairs(self.widgets) do
    local data = self.configData[w.whInfo.name]
    if (w.SetConfigData and data) then
      w:SetConfigData(data)
    end
  end
end


--------------------------------------------------------------------------------

function widgetHandler:Initialize()
  self:LoadOrderList()
  self:LoadConfigData()

  -- create the "LuaUI/Config" directory
  Spring.CreateDir('LuaUI/Config')

  local unsortedWidgets = {}
  
  -- stuff the raw widgets into unsortedWidgets
  local widgetFiles = VFS.DirList(WIDGET_DIRNAME, "*.lua", VFS.RAW_ONLY)
  for k,wf in ipairs(widgetFiles) do
    local widget = self:LoadWidget(wf, false)
    if (widget) then
      table.insert(unsortedWidgets, widget)
    end
  end

  -- stuff the zip widgets into unsortedWidgets
  local widgetFiles = VFS.DirList(WIDGET_DIRNAME, "*.lua", VFS.ZIP_ONLY)
  for k,wf in ipairs(widgetFiles) do
    local widget = self:LoadWidget(wf, true)
    if (widget) then
      table.insert(unsortedWidgets, widget)
    end
  end
  
  -- sort the widgets  
  table.sort(unsortedWidgets, function(w1, w2)
    local l1 = w1.whInfo.layer
    local l2 = w2.whInfo.layer
    if (l1 ~= l2) then
      return (l1 < l2)
    end
    local n1 = w1.whInfo.name
    local n2 = w2.whInfo.name
    local o1 = self.orderList[n1]
    local o2 = self.orderList[n2]
    if (o1 ~= o2) then
      return (o1 < o2)
    else
      return (n1 < n2)
    end
  end)

  -- add the widgets  
  for _,w in ipairs(unsortedWidgets) do
    widgetHandler:InsertWidget(w)

    local name = w.whInfo.name
    local basename = w.whInfo.basename
    print(string.format("Loaded widget:  %-18s  <%s>", name, basename))
  end

  -- save the active widgets, and their ordering
  self:SaveOrderList()
  self:SaveConfigData()
end


function widgetHandler:LoadWidget(filename, fromZip)
  local basename = Basename(filename)
  local text = VFS.LoadFile(filename)
  if (text == nil) then
    Spring.Echo('Failed to load: ' .. basename)
    return nil
  end
  local chunk, err = loadstring(text, filename)
  if (chunk == nil) then
    Spring.Echo('Failed to load: ' .. basename .. '  (' .. err .. ')')
    return nil
  end
  
  local widget = widgetHandler:NewWidget()

  -- special access for the widget selector
  if (basename == SELECTOR_BASENAME) then
    widget.widgetHandler = self
  end
  
  setfenv(chunk, widget)
  local success, err = pcall(chunk)
  if (not success) then
    Spring.Echo('Failed to load: ' .. basename .. '  (' .. err .. ')')
    return nil
  end

  self:FinalizeWidget(widget, filename, basename)
  local name = widget.whInfo.name
  if (basename == SELECTOR_BASENAME) then
    self.orderList[name] = 1  --  always enabled
  end

  err = self:ValidateWidget(widget)
  if (err) then
    Spring.Echo('Failed to load: ' .. basename .. '  (' .. err .. ')')
    return nil
  end

  local knownInfo = self.knownWidgets[name]
  if (knownInfo) then
    if (knownInfo.active) then
      print('Failed to load: ' .. basename .. '  (duplicate name)')
      return nil
    end
  else
    -- create a knownInfo table
    knownInfo = {}
    knownInfo.desc     = widget.whInfo.desc
    knownInfo.author   = widget.whInfo.author
    knownInfo.basename = widget.whInfo.basename
    knownInfo.filename = widget.whInfo.filename
    knownInfo.fromZip  = fromZip
    self.knownWidgets[name] = knownInfo
    self.knownCount = self.knownCount + 1
    self.knownChanged = true
  end
  knownInfo.active = true

  local info  = widget.GetInfo and widget:GetInfo()
  local order = self.orderList[name]
  if (((order ~= nil) and (order > 0)) or
      ((order == nil) and ((info == nil) or info.enabled))) then
    -- this will be an active widget
    if (order == nil) then
      self.orderList[name] = 12345  -- back of the pack
    else
      self.orderList[name] = order
    end
  else
    self.orderList[name] = 0
    self.knownWidgets[name].active = false
    return nil
  end

  -- load the config data  
  local config = self.configData[name]
  if (widget.SetConfigData and config) then
    widget:SetConfigData(config)
  end
    
  return widget
end


function widgetHandler:NewWidget()
  local widget = {}
  -- load the system calls into the widget table
  for k,v in pairs(System) do
    widget[k] = v
  end
  widget.WG = self.WG    -- the shared table
  widget.widget = widget -- easy self referencing

  -- wrapped calls (closures)
  widget.widgetHandler = {}
  local wh = widget.widgetHandler
  local self = self
  widget.include  = function (f) return include(f, widget) end
  wh.ForceLayout  = function (_) self:ForceLayout() end
  wh.RaiseWidget  = function (_) self:RaiseWidget(widget) end
  wh.LowerWidget  = function (_) self:LowerWidget(widget) end
  wh.RemoveWidget = function (_) self:RemoveWidget(widget) end
  wh.GetCommands  = function (_) return self.commands end
  wh.InTweakMode  = function (_) return self.tweakMode end
  wh.GetViewSizes = function (_) return self:GetViewSizes() end
  wh.GetHourTimer = function (_) return self:GetHourTimer() end
  wh.IsMouseOwner = function (_) return (self.mouseOwner == widget) end
  wh.DisownMouse  = function (_)
    if (self.mouseOwner == widget) then
      self.mouseOwner = nil
    end
  end
  wh.AddLayoutCommand = function (_, cmd)
    if (self.inCommandsChanged) then
      table.insert(self.customCommands, cmd)
    else
      Spring.Echo("AddLayoutCommand() can only be used in CommandsChanged()")
    end
  end
  wh.AddAction    = function (_, cmd, func, data, types)
    return self.actionHandler:AddAction(widget, cmd, func, data, types)
  end
  wh.RemoveAction = function (_, cmd, types)
    return self.actionHandler:RemoveAction(widget, cmd, types)
  end
  wh.ConfigLayoutHandler = function(_, d) self:ConfigLayoutHandler(d) end
  return widget
end


function widgetHandler:FinalizeWidget(widget, filename, basename)
  local wi = {}

  wi.filename = filename
  wi.basename = basename
  if (widget.GetInfo == nil) then
    wi.name  = basename
    wi.layer = 0
  else
    local info = widget:GetInfo()
    wi.name    = info.name    or basename
    wi.layer   = info.layer   or 0
    wi.desc    = info.desc    or ""
    wi.author  = info.author  or ""
    wi.license = info.license or ""
    wi.enabled = info.enabled or false
  end

  widget.whInfo = {}  --  a proxy table
  local mt = {
    __index = wi,
    __newindex = function() error("whInfo tables are read-only") end,
    __metatable = "protected"
  }
  setmetatable(widget.whInfo, mt)
end


function widgetHandler:ValidateWidget(widget)
  if (widget.GetTooltip and not widget.IsAbove) then
    return "Widget has GetTooltip() but not IsAbove()"
  end
  if (widget.TweakGetTooltip and not widget.TweakIsAbove) then
    return "Widget has TweakGetTooltip() but not TweakIsAbove()"
  end
  return nil
end


--------------------------------------------------------------------------------
--------------------------------------------------------------------------------

local function SafeWrap(func)
  local wh = widgetHandler
  return function(w, ...)
    local r = { pcall(func, w, unpack(arg)) }
    if (r[1]) then
      table.remove(r, 1)
      return unpack(r)
    else
      local name = w.whInfo.name
      widgetHandler:RemoveWidget(w)
      Spring.Echo(r[2])
      Spring.Echo('Removed widget: ' .. name)
      return nil
    end
  end
end


local function SafeWrapWidget(widget)
  if (SAFEWRAP <= 0) then
    return
  elseif (SAFEWRAP == 1) then
    if (widget.GetInfo and widget.GetInfo().unsafe) then
      Spring.Echo('LuaUI: loaded unsafe widget: ' .. widget.whInfo.name)
      return
    end
  end

  for _,ciName in callInLists do
    if (widget[ciName]) then
      widget[ciName] = SafeWrap(widget[ciName])
    end
    if (widget.Initialize) then
      widget.Initialize = SafeWrap(widget.Initialize)
    end
  end
end


function widgetHandler:InsertWidget(widget)
  if (widget == nil) then
    return
  end

  local function Insert(t, f, w)
    if (f) then
      local layer = w.whInfo.layer
      local index = 1
      for i,v in ipairs(t) do
        if (v == w) then
          return -- already in the table
        end
        if (layer >= v.whInfo.layer) then
          index = i + 1
        end
      end
      table.insert(t, index, w)
    end
  end

  SafeWrapWidget(widget)

  Insert(self.widgets, true, widget)
  for _,listname in callInLists do
    Insert(self[listname..'List'], widget[listname], widget)
  end
  self:UpdateCallIns()

  if (widget.Initialize) then
    widget:Initialize()
  end
end


function widgetHandler:RemoveWidget(widget)
  if (widget == nil) then
    return
  end

  local name = widget.whInfo.name
  if (widget.GetConfigData) then
    self.configData[name] = widget:GetConfigData()
  end
  self.knownWidgets[name].active = false
  if (widget.Shutdown) then
    widget:Shutdown()
  end
  local function Remove(t, w)
    for k,v in ipairs(t) do
      if (v == w) then
        table.remove(t, k)
        -- break
      end
    end
  end

  Remove(self.widgets, widget)
  self.actionHandler:RemoveWidgetActions(widget)
  for _,listname in callInLists do
    Remove(self[listname..'List'], widget)
  end
  self:UpdateCallIns()
end


function widgetHandler:UpdateCallIns()
  for _,name in ipairs(flexCallIns) do
    local listName = name .. 'List'
    if (table.getn(self[listName]) > 0) then
      local selffunc = self[name]
      _G[name] = function(...)
        return selffunc(self, unpack(arg))
      end
    else
      _G[name] = nil
    end
  end
end


function widgetHandler:EnableWidget(name)
  local ki = self.knownWidgets[name]
  if (not ki) then
    Spring.Echo("EnableWidget(), could not find widget: " .. tostring(name))
    return false
  end
  if (not ki.active) then
    print('Loading:  '..ki.filename)
    local order = widgetHandler.orderList[name]
    if (not order or (order <= 0)) then
      self.orderList[name] = 1
    end
    local w = self:LoadWidget(ki.filename)
    if (not w) then return false end
    self:InsertWidget(w)
    self:SaveOrderList()
  end
  return true
end


function widgetHandler:DisableWidget(name)
  local ki = self.knownWidgets[name]
  if (not ki) then
    Spring.Echo("DisableWidget(), could not find widget: " .. tostring(name))
    return false
  end
  if (ki.active) then
    local w = self:FindWidget(name)
    if (not w) then return false end
    print('Removed:  '..ki.filename)
    self:RemoveWidget(w)     -- deactivate
    self.orderList[name] = 0 -- disable
    self:SaveOrderList()
  end
  return true
end


function widgetHandler:ToggleWidget(name)
  local ki = self.knownWidgets[name]
  if (not ki) then
    Spring.Echo("ToggleWidget(), could not find widget: " .. tostring(name))
    return
  end
  if (ki.active) then
    return self:DisableWidget(name)
  elseif (self.orderList[name] <= 0) then
    return self:EnableWidget(name)
  else
    -- the widget is not active, but enabled; disable it
    self.orderList[name] = 0
    self:SaveOrderList()
  end
  return true
end


--------------------------------------------------------------------------------

local function FindWidgetIndex(t, w)
  for k,v in ipairs(t) do
    if (v == w) then
      return k
    end
  end
  return nil
end


local function FindLowestIndex(t, i, layer)
  for x = (i - 1), 1, -1 do
    if (t[x].whInfo.layer < layer) then
      return x + 1
    end
  end
  return 1
end


function widgetHandler:RaiseWidget(widget)
  if (widget == nil) then
    return
  end
  local function Raise(t, f, w)
    if (f == nil) then return end
    local i = FindWidgetIndex(t, w)
    if (i == nil) then return end
    local n = FindLowestIndex(t, i, w.whInfo.layer)
    if (n and (n < i)) then
      table.remove(t, i)
      table.insert(t, n, w)
    end
  end
  Raise(self.widgets, true, widget)
  for _,listname in callInLists do
    Raise(self[listname..'List'], widget[listname], widget)
  end
end


local function FindHighestIndex(t, i, layer)
  local ts = table.getn(t)
  for x = (i + 1),ts do
    if (t[x].whInfo.layer > layer) then
      return (x - 1)
    end
  end
  return (ts + 1)
end


function widgetHandler:LowerWidget(widget)
  if (widget == nil) then
    return
  end
  local function Lower(t, f, w)
    if (f == nil) then return end
    local i = FindWidgetIndex(t, w)
    if (i == nil) then return end
    local n = FindHighestIndex(t, i, w.whInfo.layer)
    if (n and (n > i)) then
      table.insert(t, n, w)
      table.remove(t, i)
    end
  end
  Lower(self.widgets, true, widget)
  for _,listname in callInLists do
    Lower(self[listname..'List'], widget[listname], widget)
  end
end


function widgetHandler:FindWidget(name)
  if (type(name) ~= 'string') then
    return nil
  end
  for k,v in ipairs(self.widgets) do
    if (name == v.whInfo.name) then
      return v,k
    end
  end
  return nil
end


--------------------------------------------------------------------------------
--------------------------------------------------------------------------------
--
--  Helper facilities
--

local hourTimer = 0


function widgetHandler:GetHourTimer()
  return hourTimer
end


function widgetHandler:GetViewSizes()
  return self.xViewSize, self.yViewSize
end


function widgetHandler:ForceLayout()
  forceLayout = true  --  in main.lua
end


function widgetHandler:ConfigLayoutHandler(data)
  ConfigLayoutHandler(data)
end


--------------------------------------------------------------------------------
--------------------------------------------------------------------------------
--
--  The call-in distribution routines
--

function widgetHandler:Shutdown()
  self:SaveOrderList()
  self:SaveConfigData()
  for _,w in ipairs(self.ShutdownList) do
    w:Shutdown()
  end
  return
end

function widgetHandler:Update()
  local deltaTime = Spring.GetLastUpdateSeconds()  
  -- update the hour timer
  hourTimer = math.mod(hourTimer + deltaTime, 3600.0)
  for _,w in ipairs(self.UpdateList) do
    w:Update(deltaTime)
  end
  return
end


function widgetHandler:ConfigureLayout(command)
  if (command == 'tweakgui') then
    self.tweakMode = true
    Spring.Echo("LuaUI TweakMode: ON")
    return true
  elseif (command == 'reconf') then
    self:SendConfigData()
    return true
  elseif (command == 'selector') then
    for _,w in ipairs(self.widgets) do
      if (w.whInfo.basename == SELECTOR_BASENAME) then
        return true  -- there can only be one
      end
    end
    local sw = self:LoadWidget(LUAUI_DIRNAME .. SELECTOR_BASENAME)
    self:InsertWidget(sw)
    self:RaiseWidget(sw)
    return true
  elseif (string.find(command, 'togglewidget') == 1) then
    self:ToggleWidget(string.sub(command, 14))
    return true
  elseif (string.find(command, 'enablewidget') == 1) then
    self:EnableWidget(string.sub(command, 14))
    return true
  elseif (string.find(command, 'disablewidget') == 1) then
    self:DisableWidget(string.sub(command, 15))
    return true
  end

  if (self.actionHandler:TextAction(command)) then
    return true
  end

  for _,w in ipairs(self.TextCommandList) do
    if (w:TextCommand(command)) then
      return true
    end
  end
  return false
end


function widgetHandler:CommandNotify(id, params, options)
  for _,w in ipairs(self.CommandNotifyList) do
    if (w:CommandNotify(id, params, options)) then
      return true
    end
  end
  return false
end


function widgetHandler:AddConsoleLine(msg, priority)
  for _,w in ipairs(self.AddConsoleLineList) do
    w:AddConsoleLine(msg, priority)
  end
  return
end


function widgetHandler:GroupChanged(groupID)
  for _,w in ipairs(self.GroupChangedList) do
    w:GroupChanged(groupID)
  end
  return
end


function widgetHandler:CommandsChanged()
  self.inCommandsChanged = true
  self.customCommands = {}
  for _,w in ipairs(self.CommandsChangedList) do
    w:CommandsChanged()
  end
  self.inCommandsChanged = false
  return
end


--------------------------------------------------------------------------------
--
--  Drawing call-ins
--

-- generates ViewResize() calls for the widgets
function widgetHandler:SetViewSize(vsx, vsy)
  self.xViewSize = vsx
  self.yViewSize = vsy
  if ((self.xViewSizeOld ~= vsx) or
      (self.yViewSizeOld ~= vsy)) then
    widgetHandler:ViewResize(vsx, vsy)
    self.xViewSizeOld = vsx
    self.yViewSizeOld = vsy
  end
end


function widgetHandler:ViewResize(vsx, vsy)
  for _,w in ipairs(self.ViewResizeList) do
    w:ViewResize(vsx, vsy)
  end
  return
end


function widgetHandler:DrawWorldItems()
  for _,w in ripairs(self.DrawWorldList) do
    w:DrawWorld()
  end
  return
end


function widgetHandler:DrawScreenItems()
  if (self.tweakMode) then
    gl.Color(0, 0, 0, 0.5)
    local sx, sy = self.xViewSize, self.yViewSize
    gl.Shape(GL.QUADS, {
      {v = {  0,  0 }}, {v = { sx,  0 }}, {v = { sx, sy }}, {v = {  0, sy }}
    })
    gl.Color(1, 1, 1)
  end
  for _,w in ripairs(self.DrawScreenList) do
    w:DrawScreen()
    if (self.tweakMode and w.TweakDrawScreen) then
      w:TweakDrawScreen()
    end
  end
end


function widgetHandler:DrawWorldShadow()
  for _,w in ipairs(self.DrawWorldShadowList) do
    w:DrawWorldShadow()
  end
  return
end


function widgetHandler:DrawWorldReflection()
  for _,w in ipairs(self.DrawWorldReflectionList) do
    w:DrawWorldReflection()
  end
  return
end


function widgetHandler:DrawWorldRefraction()
  for _,w in ipairs(self.DrawWorldRefractionList) do
    w:DrawWorldRefraction()
  end
  return
end


function widgetHandler:DrawInMiniMap(xSize, ySize)
  for _,w in ipairs(self.DrawInMiniMapList) do
    w:DrawInMiniMap(xSize, ySize)
  end
  return
end


--------------------------------------------------------------------------------
--
--  Keyboard call-ins
--

function widgetHandler:KeyPress(key, mods, isRepeat)
  if (self.actionHandler:KeyAction(true, key, mods, isRepeat)) then
    return true
  end

  if (self.tweakMode) then
    local mo = self.mouseOwner
    if (mo and mo.TweakKeyPress) then
      mo:TweakKeyPress(key, mods, isRepeat)
    end
    return true
  end
  for _,w in ipairs(self.KeyPressList) do
    if (w:KeyPress(key, mods, isRepeat)) then
      return true
    end
  end
  return false
end


function widgetHandler:KeyRelease(key, mods)
  if (self.actionHandler:KeyAction(false, key, mods, false)) then
    return true
  end

  if (self.tweakMode) then
    local mo = self.mouseOwner
    if (mo and mo.TweakKeyRelease) then
      mo:TweakKeyRelease(key, mods)
    elseif (key == KEYSYMS.ESCAPE) then
      Spring.Echo("LuaUI TweakMode: OFF")
      self.tweakMode = false
    end
    return true
  end
  for _,w in ipairs(self.KeyReleaseList) do
    if (w:KeyRelease(key, mods)) then
      return true
    end
  end
  return false
end


--------------------------------------------------------------------------------
--
--  Mouse call-ins
--

-- local helper (not a real call-in)
function widgetHandler:WidgetAt(x, y)
  if (not self.tweakMode) then
    for _,w in ipairs(self.IsAboveList) do
      if (w:IsAbove(x, y)) then
        return w
      end
    end
  else
    for _,w in ipairs(self.TweakIsAboveList) do
      if (w:TweakIsAbove(x, y)) then
        return w
      end
    end
  end
  return nil
end


function widgetHandler:MousePress(x, y, button)
  local mo = self.mouseOwner
  if (not self.tweakMode) then
    for _,w in ipairs(self.MousePressList) do
      if (w:MousePress(x, y, button)) then
        self.mouseOwner = w
        return true
      end
    end
    return false
  else
    if (mo) then
      mo:TweakMousePress(x, y, button)
      return true  --  already have an active press
    end
    for _,w in ipairs(self.TweakMousePressList) do
      if (w:TweakMousePress(x, y, button)) then
        self.mouseOwner = w
        return true
      end
    end
    return true  --  always grab the mouse
  end
end


function widgetHandler:MouseMove(x, y, dx, dy, button)
  local mo = self.mouseOwner
  if (not self.tweakMode) then
    if (mo and mo.MouseMove) then
      return mo:MouseMove(x, y, dx, dy, button)
    end
  else
    if (mo and mo.TweakMouseMove) then
      mo:TweakMouseMove(x, y, dx, dy, button)
    end
    return true
  end
end


function widgetHandler:MouseRelease(x, y, button)
  local mo = self.mouseOwner
  self.mouseOwner = nil
  if (not self.tweakMode) then
    if (mo and mo.MouseRelease) then
      return mo:MouseRelease(x, y, button)
    end
    return -1
  else
    if (mo and mo.TweakMouseRelease) then
      mo:TweakMouseRelease(x, y, button)
    end
    return -1
  end
end


function widgetHandler:IsAbove(x, y)
  if (self.tweakMode) then
    return true
  end
  return (widgetHandler:WidgetAt(x, y) ~= nil)
end


function widgetHandler:GetTooltip(x, y)
  if (not self.tweakMode) then
    for _,w in ipairs(self.GetTooltipList) do
      if (w:IsAbove(x, y)) then
        local tip = w:GetTooltip(x, y)
        if (string.len(tip) > 0) then
          return tip
        end
      end
    end
    return ""
  else
    for _,w in ipairs(self.TweakGetTooltipList) do
      if (w:TweakIsAbove(x, y)) then
        local tip = w:TweakGetTooltip(x, y)
        if (string.len(tip) > 0) then
          return tip
        end
      end
    end
    return "Tweak Mode  --  hit ESCAPE to cancel"
  end
end


--------------------------------------------------------------------------------
--
--  Game call-ins
--

function widgetHandler:GameOver()
  for _,w in ipairs(self.GameOverList) do
    w:GameOver()
  end
  return
end


function widgetHandler:TeamDied(teamID)
  for _,w in ipairs(self.TeamDiedList) do
    w:TeamDied(teamID)
  end
  return
end


--------------------------------------------------------------------------------
--
--  Unit call-ins
--

function widgetHandler:UnitCreated(unitID, unitDefID, unitTeam)
  for _,w in ipairs(self.UnitCreatedList) do
    w:UnitCreated(unitID, unitDefID, unitTeam)
  end
  return
end


function widgetHandler:UnitFinished(unitID, unitDefID, unitTeam)
  for _,w in ipairs(self.UnitFinishedList) do
    w:UnitFinished(unitID, unitDefID, unitTeam)
  end
  return
end


function widgetHandler:UnitFromFactory(unitID, unitDefID, unitTeam,
                                       factID, factDefID, userOrders)
  for _,w in ipairs(self.UnitFromFactoryList) do
    w:UnitFromFactory(unitID, unitDefID, unitTeam,
                      factID, factDefID, userOrders)
  end
  return
end


function widgetHandler:UnitDestroyed(unitID, unitDefID, unitTeam)
  for _,w in ipairs(self.UnitDestroyedList) do
    w:UnitDestroyed(unitID, unitDefID, unitTeam)
  end
  return
end


function widgetHandler:UnitIdle(unitID, unitDefID, unitTeam)
  for _,w in ipairs(self.UnitIdleList) do
    w:UnitIdle(unitID, unitDefID, unitTeam)
  end
  return
end


function widgetHandler:UnitTaken(unitID, unitDefID, unitTeam, newTeam)
  for _,w in ipairs(self.UnitTakenList) do
    w:UnitTaken(unitID, unitDefID, unitTeam, newTeam)
  end
  return
end


function widgetHandler:UnitGiven(unitID, unitDefID, unitTeam, oldTeam)
  for _,w in ipairs(self.UnitGivenList) do
    w:UnitGiven(unitID, unitDefID, unitTeam, oldTeam)
  end
  return
end


function widgetHandler:UnitEnteredRadar(unitID, unitTeam)
  for _,w in ipairs(self.UnitEnteredRadarList) do
    w:UnitEnteredRadar(unitID, unitTeam)
  end
  return
end


function widgetHandler:UnitEnteredLos(unitID, unitDefID, unitTeam)
  for _,w in ipairs(self.UnitEnteredLosList) do
    w:UnitEnteredLos(unitID, unitDefID, unitTeam)
  end
  return
end


function widgetHandler:UnitLeftRadar(unitID, unitTeam)
  for _,w in ipairs(self.UnitLeftRadarList) do
    w:UnitLeftRadar(unitID, unitTeam)
  end
  return
end


function widgetHandler:UnitLeftLos(unitID, unitDefID, unitTeam)
  for _,w in ipairs(self.UnitLeftLosList) do
    w:UnitLeftLos(unitID, unitDefID, unitTeam)
  end
  return
end


function widgetHandler:UnitSeismicPing(x, y, z, strength)
  for _,w in ipairs(self.UnitSeismicPingList) do
    w:UnitSeismicPing(x, y, z, strength)
  end
  return
end


--------------------------------------------------------------------------------
--------------------------------------------------------------------------------
