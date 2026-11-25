#pragma once
// Linux-only X11 native host test. GUI thread owns registration, event pump and view.
class LinuxFrame final : public FObject, public IPlugFrame, public Steinberg::Linux::IRunLoop {
    using Clock=std::chrono::steady_clock;
    struct FD { IPtr<Steinberg::Linux::IEventHandler> handler; int fd; };
    struct Timer { IPtr<Steinberg::Linux::ITimerHandler> handler; std::chrono::milliseconds interval; Clock::time_point next; };
    std::vector<FD> fds;
    std::vector<Timer> timers;
    Display* display; Window parent;
public:
    LinuxFrame(Display* d,Window w):display(d),parent(w){}
    void setParent(Window w){parent=w;}
    tresult PLUGIN_API resizeView(IPlugView* view,ViewRect* r) override{
        if(!view||!r||r->getWidth()<=0||r->getHeight()<=0)return kInvalidArgument;
        XResizeWindow(display,parent,r->getWidth(),r->getHeight());XSync(display,False);
        return view->onSize(r);
    }
    tresult PLUGIN_API registerEventHandler(Steinberg::Linux::IEventHandler* h,Steinberg::Linux::FileDescriptor fd) override{
        if(!h||fd<0)return kInvalidArgument;
        for(auto& f:fds)if(f.handler.get()==h)return kResultFalse;
        fds.push_back({IPtr<Steinberg::Linux::IEventHandler>(h),fd});return kResultOk;
    }
    tresult PLUGIN_API unregisterEventHandler(Steinberg::Linux::IEventHandler* h) override{
        auto i=std::find_if(fds.begin(),fds.end(),[&](const FD& f){return f.handler.get()==h;});
        if(i==fds.end())return kResultFalse;fds.erase(i);return kResultOk;
    }
    tresult PLUGIN_API registerTimer(Steinberg::Linux::ITimerHandler* h,Steinberg::Linux::TimerInterval ms) override{
        if(!h||!ms||ms>600000)return kInvalidArgument;
        for(auto& t:timers)if(t.handler.get()==h)return kResultFalse;
        auto interval=std::chrono::milliseconds(ms);
        timers.push_back({IPtr<Steinberg::Linux::ITimerHandler>(h),interval,Clock::now()+interval});return kResultOk;
    }
    tresult PLUGIN_API unregisterTimer(Steinberg::Linux::ITimerHandler* h) override{
        auto i=std::find_if(timers.begin(),timers.end(),[&](const Timer& t){return t.handler.get()==h;});
        if(i==timers.end())return kResultFalse;timers.erase(i);return kResultOk;
    }
    void pump(int ms){
        const auto end=Clock::now()+std::chrono::milliseconds(ms);
        do{
            auto snapshot=fds;
            for(auto& f:snapshot){
                pollfd p{f.fd,POLLIN,0};
                if(::poll(&p,1,0)>0&&(p.revents&POLLIN)&&std::any_of(fds.begin(),fds.end(),[&](const FD& a){return a.handler.get()==f.handler.get()&&a.fd==f.fd;}))
                    f.handler->onFDIsSet(f.fd);
            }
            auto now=Clock::now();
            std::vector<IPtr<Steinberg::Linux::ITimerHandler>> ready;
            for(auto& t:timers)if(t.next<=now){ready.push_back(t.handler);t.next=now+t.interval;}
            for(auto& h:ready)if(std::any_of(timers.begin(),timers.end(),[&](const Timer& t){return t.handler.get()==h.get();}))h->onTimer();
            while(XPending(display)){XEvent e;XNextEvent(display,&e);}
            XFlush(display);std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }while(Clock::now()<end);
    }
    OBJ_METHODS(LinuxFrame,FObject)
    DEFINE_INTERFACES
        DEF_INTERFACE(IPlugFrame)
        DEF_INTERFACE(Steinberg::Linux::IRunLoop)
    END_DEFINE_INTERFACES(FObject)
    REFCOUNT_METHODS(FObject)
};
class LinuxHandler final : public FObject, public IComponentHandler {
public:
    Native* relay=nullptr;
    std::array<int,14> begins{},edits{},ends{};
    tresult PLUGIN_API beginEdit(ParamID id) override{if(id<100||id>113)return kInvalidArgument;++begins[id-100];return kResultOk;}
    tresult PLUGIN_API performEdit(ParamID id,ParamValue v) override{
        if(id<100||id>113||!std::isfinite(v)||v<0||v>1)return kInvalidArgument;
        ++edits[id-100];if(relay){ParameterChanges p(14);point(p,id,0,v);relay->flush(p,kSample64);}return kResultOk;
    }
    tresult PLUGIN_API endEdit(ParamID id) override{if(id<100||id>113)return kInvalidArgument;++ends[id-100];return kResultOk;}
    tresult PLUGIN_API restartComponent(int32) override{return kResultOk;}
    OBJ_METHODS(LinuxHandler,FObject)
    DEFINE_INTERFACES
        DEF_INTERFACE(IComponentHandler)
    END_DEFINE_INTERFACES(FObject)
    REFCOUNT_METHODS(FObject)
};
unsigned int childCount(Display* d,Window w){
    Window root{},parent{},*children=nullptr;unsigned int count=0;
    require(XQueryTree(d,w,&root,&parent,&children,&count)!=0,"X11 child query");
    if(children)XFree(children);return count;
}
// XEmbed host responsibility: VSTGUI creates an *unmapped* child. The
// host must read _XEMBED_INFO, notify the child and map it, as in the
// pinned SDK editorhost Linux window.cpp CreateNotify handler.
void embedChild(Display* d,Window parent){
    Window root{},p{},*children=nullptr;unsigned int count=0;
    require(XQueryTree(d,parent,&root,&p,&children,&count)!=0&&count==1,"exact XEmbed child");
    const Window child=children[0];XFree(children);
    const Atom info=XInternAtom(d,"_XEMBED_INFO",True),embed=XInternAtom(d,"_XEMBED",True);
    require(info!=None&&embed!=None,"XEmbed atoms");
    Atom type{};int format=0;unsigned long items=0,remaining=0;unsigned char* data=nullptr;
    require(XGetWindowProperty(d,child,info,0,2,False,info,&type,&format,&items,&remaining,&data)==Success&&
        type==info&&format==32&&items==2&&data,"XEmbed info property");
    const auto version=reinterpret_cast<unsigned long*>(data)[0];XFree(data);
    auto send=[&](long opcode,long detail,long value1,long value2){
        XEvent e{};e.xclient.type=ClientMessage;e.xclient.window=child;e.xclient.message_type=embed;e.xclient.format=32;
        e.xclient.data.l[0]=CurrentTime;e.xclient.data.l[1]=opcode;e.xclient.data.l[2]=detail;
        e.xclient.data.l[3]=value1;e.xclient.data.l[4]=value2;
        require(XSendEvent(d,child,False,NoEventMask,&e)!=0,"XEmbed notify");XSync(d,False);
    };
    send(0,0,parent,version);XMapWindow(d,child);XResizeWindow(d,child,960,576);XSync(d,False);
    send(1,0,0,0);send(4,0,0,0);
    XWindowAttributes a{};require(XGetWindowAttributes(d,child,&a)!=0&&a.map_state==IsViewable,"XEmbed child mapped");
    std::cerr<<"LINUX_XEMBED_MAPPED child="<<child<<"\n";
}
void linuxEditorProbe(const VST3::Hosting::Module::Ptr& module,const std::filesystem::path& folder,bool negative){
    auto* d=XOpenDisplay(nullptr);require(d,"X11 display");
    // Bare Xvfb has no desktop host to establish these protocol atoms.
    // XInternAtom(False) creates them if absent; a subsequent lookup is
    // still required by embedChild, including the child's actual property.
    require(XInternAtom(d,"_XEMBED_INFO",False)!=None&&
        XInternAtom(d,"_XEMBED",False)!=None,"initialize XEmbed protocol atoms");
    XSync(d,False);
    Native n(module);auto handler=owned(new LinuxHandler);n.start(48000,kSample64,2);
    handler->relay=&n;require(n.controller->setComponentHandler(handler.get())==kResultOk,"Linux host handler");
    const auto baseline=n.save();
    // One host event loop serves every editor, including after detach/reopen.
    auto frame=owned(new LinuxFrame(d,0));
    for(int cycle=0;cycle<(negative?1:20);++cycle){
        Window w=XCreateSimpleWindow(d,DefaultRootWindow(d),30,30,960,576,0,0,0);
        XMapRaised(d,w);XSync(d,False);
        frame->setParent(w);auto view=owned(n.controller->createView(ViewType::kEditor));
        require(bool(view),"Linux editor creation");
        if(cycle==0&&!negative){
            // Factory context is the ordinary HostApplication without IRunLoop.
            // No frame must fail safely instead of entering the SDK's null runloop.
            require(view->attached(reinterpret_cast<void*>(static_cast<uintptr_t>(w)),kPlatformTypeX11EmbedWindowID)!=kResultOk&&childCount(d,w)==0,"missing runloop rejected");
        }
        require(view->setFrame(frame.get())==kResultOk,"Linux editor frame");
        std::cerr<<"LINUX_ATTACH_BEGIN cycle="<<cycle<<"\n";
        require(view->isPlatformTypeSupported(kPlatformTypeX11EmbedWindowID)==kResultOk,"X11 platform");
        auto result=view->attached(reinterpret_cast<void*>(static_cast<uintptr_t>(w)),kPlatformTypeX11EmbedWindowID);
        std::cerr<<"LINUX_ATTACH_RETURN result="<<result<<"\n";
        if(negative){require(result!=kResultOk&&childCount(d,w)==0,"corrupt Linux resource rejects native attach");}
        else{
            require(result==kResultOk,"Linux X11 attach");embedChild(d,w);frame->pump(300);require(childCount(d,w)>0,"native child window");
            if(cycle==0){
                ViewRect r{};require(view->getSize(&r)==kResultOk&&r.getWidth()==960&&r.getHeight()==576,"initial geometry");
                XSync(d,False);
                Window child{};int rootX=0,rootY=0;require(XTranslateCoordinates(d,w,DefaultRootWindow(d),0,0,&rootX,&rootY,&child)!=0,"screen capture coordinates");
                XImage* image=XGetImage(d,DefaultRootWindow(d),rootX,rootY,960,576,AllPlanes,ZPixmap);require(image,"actual native X11 screenshot");
                auto channel=[](unsigned long v,unsigned long mask){if(!mask)return 0u;while(!(mask&1)){mask>>=1;v>>=1;}return static_cast<unsigned int>((v&mask)*255/mask);};
                std::filesystem::create_directories(folder);
                std::ofstream ppm(folder/"native-editor.ppm",std::ios::binary);ppm<<"P6\n960 576\n255\n";
                int orange=0;for(int y=0;y<576;++y)for(int x=0;x<960;++x){
                    auto pixel=XGetPixel(image,x,y);unsigned char rgb[]{static_cast<unsigned char>(channel(pixel,image->red_mask)),static_cast<unsigned char>(channel(pixel,image->green_mask)),static_cast<unsigned char>(channel(pixel,image->blue_mask))};
                    ppm.write(reinterpret_cast<char*>(rgb),3);
                    if(y>80&&y<165&&x>24&&x<936&&rgb[0]>190&&rgb[1]>95&&rgb[1]<195&&rgb[2]<110)++orange;
                }
                XDestroyImage(image);ppm.close();std::cerr<<"LINUX_DISPLAY_ORANGE_PIXELS "<<orange<<"\n";require(ppm.good()&&orange>500,"nonblank orange timing display");
                auto click=[&](int x,int y){Window child{};int rx=0,ry=0;XTranslateCoordinates(d,w,DefaultRootWindow(d),x,y,&rx,&ry,&child);
                    XTestFakeMotionEvent(d,DefaultScreen(d),rx,ry,CurrentTime);XTestFakeButtonEvent(d,1,True,CurrentTime);XSync(d,False);frame->pump(50);
                    XTestFakeButtonEvent(d,1,False,CurrentTime);XSync(d,False);frame->pump(80);};
                click(650,34);require(n.controller->getParamNormalized(111)==1&&handler->edits[11]>0,"actual ACTIVE ON click");
                // The default pattern begins open. Wait a full second of actual PCM;
                // a one-block probe would incorrectly fail the approved default pattern.
                double minGain=1.,maxGain=0.;
                for(int i=0;i<100;++i)for(auto v:n.render<double>(512)){minGain=std::min(minGain,v);maxGain=std::max(maxGain,v);}
                require(minGain<.1&&maxGain>.99,"GUI ACTIVE audible PCM");
                click(573,34);require(n.controller->getParamNormalized(111)==0,"actual ACTIVE OFF click");
                click(878,34);require(n.controller->getParamNormalized(112)==1&&handler->edits[12]>0,"actual LOOP click");
                click(762,34);require(n.controller->getParamNormalized(112)==0,"actual ONE SHOT click");
                click(845,546);require(n.controller->getParamNormalized(106)==1&&handler->edits[6]>0,"actual SINE click");
                click(115,546);require(n.controller->getParamNormalized(106)==0,"actual RECT click");
                for(auto id:{6,11,12})require(handler->begins[id]>0&&handler->begins[id]==handler->ends[id],"balanced native GUI automation");
                ViewRect large{0,0,1200,720};require(frame->resizeView(view.get(),&large)==kResultOk,"native resize request");frame->pump(100);
                require(view->getSize(&r)==kResultOk&&r.getWidth()==1200&&r.getHeight()==720,"native resized editor geometry");
                ViewRect normal{0,0,960,576};require(frame->resizeView(view.get(),&normal)==kResultOk,"native resize restore");
                frame->pump(60);
                require(n.save()==baseline,"restored GUI controls preserve component state");
            }
            std::cerr<<"LINUX_DETACH_BEGIN cycle="<<cycle<<"\n";
            require(view->removed()==kResultOk,"Linux editor detach");frame->pump(50);XSync(d,False);
            require(childCount(d,w)==0,"native child destroyed");
        }
        require(view->setFrame(nullptr)==kResultOk,"clear Linux frame");
        XDestroyWindow(d,w);XSync(d,False);
        require(n.save()==baseline,"Linux editor lifecycle no state mutation");
    }
    handler->relay=nullptr;require(n.controller->setComponentHandler(nullptr)==kResultOk,"clear Linux host handler");
    n.stop();frame->setParent(0);XCloseDisplay(d);
    std::cout<<(negative?"LINUX_RESOURCE_REJECT_PASS":"LINUX_X11_EDITOR_20_CYCLES_SWITCH_PCM_RESIZE_PASS")<<"\n";
}
