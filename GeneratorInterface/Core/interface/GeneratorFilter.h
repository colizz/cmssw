// -*- C++ -*-
//
//

// class template GeneratorFilter<HAD> provides an EDFilter which uses
// the hadronizer type HAD to generate partons, hadronize them, and
// decay the resulting particles, in the CMS framework.

#ifndef gen_GeneratorFilter_h
#define gen_GeneratorFilter_h

#include <memory>
#include <string>
#include <vector>

#include "FWCore/Concurrency/interface/SharedResourceNames.h"
#include "FWCore/Framework/interface/one/EDFilter.h"
#include "FWCore/Framework/interface/Event.h"
#include "FWCore/Framework/interface/EventSetup.h"
#include "FWCore/Framework/interface/FileBlock.h"
#include "FWCore/Framework/interface/LuminosityBlock.h"
#include "FWCore/Framework/interface/MakerMacros.h"
#include "FWCore/Framework/interface/Run.h"
#include "FWCore/ParameterSet/interface/ParameterSet.h"
#include "FWCore/ServiceRegistry/interface/RandomEngineSentry.h"
#include "FWCore/Utilities/interface/EDMException.h"
#include "CLHEP/Random/RandomEngine.h"

// #include "GeneratorInterface/ExternalDecays/interface/ExternalDecayDriver.h"

//#include "GeneratorInterface/LHEInterface/interface/LHEEvent.h"
#include "SimDataFormats/GeneratorProducts/interface/HepMCProduct.h"
#include "SimDataFormats/GeneratorProducts/interface/GenRunInfoProduct.h"
#include "SimDataFormats/GeneratorProducts/interface/GenLumiInfoHeader.h"
#include "SimDataFormats/GeneratorProducts/interface/GenLumiInfoProduct.h"
#include "SimDataFormats/GeneratorProducts/interface/GenEventInfoProduct.h"

namespace edm
{
  template <class HAD, class DEC> class GeneratorFilter : public one::EDFilter<EndRunProducer,
                                                                               BeginLuminosityBlockProducer,
									       EndLuminosityBlockProducer,
                                                                               one::WatchLuminosityBlocks,
                                                                               one::SharedResources>
  {
  public:
    typedef HAD Hadronizer;
    typedef DEC Decayer;

    // The given ParameterSet will be passed to the contained
    // Hadronizer object.
    explicit GeneratorFilter(ParameterSet const& ps);

    ~GeneratorFilter() override;

    bool filter(Event& e, EventSetup const& es) override;
    void endRunProduce(Run &, EventSetup const&) override;
    void beginLuminosityBlock(LuminosityBlock const&, EventSetup const&) override;
    void beginLuminosityBlockProduce(LuminosityBlock&, EventSetup const&) override;
    void endLuminosityBlock(LuminosityBlock const&, EventSetup const&) override;
    void endLuminosityBlockProduce(LuminosityBlock &, EventSetup const&) override;
    void preallocThreads(unsigned int iThreads) override;

  private:
    Hadronizer            hadronizer_;
    //gen::ExternalDecayDriver* decayer_;
    Decayer*              decayer_;
    unsigned int          nEventsInLumiBlock_;
    unsigned int          nThreads_{1};
    bool                  initialized_ = false;
  };

  //------------------------------------------------------------------------
  //
  // Implementation

  template <class HAD, class DEC>
  GeneratorFilter<HAD,DEC>::GeneratorFilter(ParameterSet const& ps) :
    EDFilter(),
    hadronizer_(ps),
    decayer_(nullptr),
    nEventsInLumiBlock_(0)
  {
    // TODO:
    // Put the list of types produced by the filters here.
    // The current design calls for:
    //   * GenRunInfoProduct
    //   * HepMCProduct
    //
    // other maybe added as needs be
    //

    std::vector<std::string> const& sharedResources = hadronizer_.sharedResources();
    for(auto const& resource : sharedResources) {
      usesResource(resource);
    }

    if ( ps.exists("ExternalDecays") )
    {
       //decayer_ = new gen::ExternalDecayDriver(ps.getParameter<ParameterSet>("ExternalDecays"));
       ParameterSet ps1 = ps.getParameter<ParameterSet>("ExternalDecays");
       decayer_ = new Decayer(ps1);

       std::vector<std::string> const& sharedResourcesDec = decayer_->sharedResources();
       for(auto const& resource : sharedResourcesDec) {
         usesResource(resource);
       }
    }
    
    // This handles the case where there are no shared resources, because you
    // have to declare something when the SharedResources template parameter was used.
    if(sharedResources.empty() && (!decayer_ || decayer_->sharedResources().empty())) {
      usesResource(edm::uniqueSharedResourceName());
    }

    produces<edm::HepMCProduct>("unsmeared");
    produces<GenEventInfoProduct>();
    produces<GenLumiInfoHeader, edm::Transition::BeginLuminosityBlock>();
    produces<GenLumiInfoProduct, edm::Transition::EndLuminosityBlock>();
    produces<GenRunInfoProduct, edm::Transition::EndRun>();
 
  }

  template <class HAD, class DEC>
  GeneratorFilter<HAD, DEC>::~GeneratorFilter()
  { if ( decayer_ ) delete decayer_;}

  template <class HAD, class DEC>
  void
  GeneratorFilter<HAD, DEC>::preallocThreads(unsigned int iThreads) {
    nThreads_ = iThreads;
  }

  template <class HAD, class DEC>
  bool
  GeneratorFilter<HAD, DEC>::filter(Event& ev, EventSetup const& /* es */)
  {
    RandomEngineSentry<HAD> randomEngineSentry(&hadronizer_, ev.streamID());
    RandomEngineSentry<DEC> randomEngineSentryDecay(decayer_, ev.streamID());

    //added for selecting/filtering gen events, in the case of hadronizer+externalDecayer
    Service<RandomNumberGenerator> rng;
    auto &engine = rng->getEngine(ev.streamID());
    engine.setSeed(123456789L, 0);
    engine.get({2888826676L, 4222204286L, 239210476L, 2844405951L, 35428347L, 354167893L, 78655686L, 2729890738L, 145928944L, 2854232373L, 8374007L, 604295913L, 98961718L, 3463364680L, 217399053L, 1172894265L, 476173365L, 581365486L, 87742339L, 3794171095L, 467348746L, 1977749280L, 374460165L, 3619523079L, 300465183L, 67791381L, 415197038L, 4104105407L, 109639045L, 2293915619L, 218460587L, 782512343L, 520213210L, 3314195479L, 228207194L, 4L, 126079611L, 263768728L});
    for (auto &i : engine.put()) {std::cout << i << " ";} std::cout << " -//- " <<  engine.getSeed() << "\n";
      
    bool passEvtGenSelector = false;
    std::unique_ptr<HepMC::GenEvent> event(nullptr);
   
    while(!passEvtGenSelector)
      {
	event.reset();
	hadronizer_.setEDMEvent(ev);
	
	if ( !hadronizer_.generatePartonsAndHadronize() ) return false;
	
	//  this is "fake" stuff
	// in principle, decays are done as part of full event generation,
	// except for particles that are marked as to be kept stable
	// but we currently keep in it the design, because we might want
	// to use such feature for other applications
	//
	if ( !hadronizer_.decay() ) return false;
	
	event = hadronizer_.getGenEvent();
	if ( !event.get() ) return false; 
	
	// The external decay driver is being added to the system,
	// it should be called here
	//
	if ( decayer_ ) 
	{
           auto t = decayer_->decay( event.get() );
           if(t != event.get()) {
             event.reset(t);
           }
	}
	if ( !event.get() ) return false;
	
	passEvtGenSelector = hadronizer_.select( event.get() );
	
      }
    // check and perform if there're any unstable particles after 
    // running external decay packages
    //
    // fisrt of all, put back modified event tree (after external decay)
    //
    hadronizer_.resetEvent( std::move(event) );
	
    //
    // now run residual decays
    //
    if ( !hadronizer_.residualDecay() ) return false;
    	
    hadronizer_.finalizeEvent();
    
    event =  hadronizer_.getGenEvent();
    if ( !event.get() ) return false;
    
    event->set_event_number( ev.id().event() );
    
    //
    // tutto bene - finally, form up EDM products !
    //
    auto genEventInfo = hadronizer_.getGenEventInfo();
    if (!genEventInfo.get())
      { 
	// create GenEventInfoProduct from HepMC event in case hadronizer didn't provide one
	genEventInfo.reset(new GenEventInfoProduct(event.get()));
      }
      
    ev.put(std::move(genEventInfo));
   
    std::unique_ptr<HepMCProduct> bare_product(new HepMCProduct());
    bare_product->addHepMCData( event.release() );
    ev.put(std::move(bare_product), "unsmeared");
    nEventsInLumiBlock_ ++;
    return true;
  }

  template <class HAD, class DEC>
  void
  GeneratorFilter<HAD, DEC>::endRunProduce( Run& r, EventSetup const& )
  {
    // If relevant, record the integrated luminosity for this run
    // here.  To do so, we would need a standard function to invoke on
    // the contained hadronizer that would report the integrated
    // luminosity.

    if(initialized_) {
      hadronizer_.statistics();
    
      if ( decayer_ ) decayer_->statistics();
    }
    
    std::unique_ptr<GenRunInfoProduct> griproduct(new GenRunInfoProduct(hadronizer_.getGenRunInfo()));
    r.put(std::move(griproduct));
  }

  template <class HAD, class DEC>
  void
  GeneratorFilter<HAD, DEC>::beginLuminosityBlock( LuminosityBlock const& lumi, EventSetup const& es )
  {}
  
  template <class HAD, class DEC>
  void
  GeneratorFilter<HAD, DEC>::beginLuminosityBlockProduce( LuminosityBlock &lumi, EventSetup const& es )
  {
    nEventsInLumiBlock_ = 0;
    RandomEngineSentry<HAD> randomEngineSentry(&hadronizer_, lumi.index());
    RandomEngineSentry<DEC> randomEngineSentryDecay(decayer_, lumi.index());

    Service<RandomNumberGenerator> rng;
    auto &engine = rng->getEngine(lumi.index());
    engine.setSeed(123456789L, 0);
    engine.get({2888826676L, 1086689750L, 464451294L, 3678444570L, 262261918L, 2027866678L, 183391890L, 3405079271L, 518542718L, 986412739L, 228981659L, 2742392937L, 153984327L, 1869421268L, 325378421L, 3688294823L, 65069892L, 149953528L, 350481243L, 113987349L, 393015960L, 901925528L, 288444849L, 1638152900L, 10519736L, 1145145174L, 119115892L, 1557136059L, 246838302L, 725415457L, 482492383L, 1179577351L, 84320973L, 3931436850L, 386528827L, 17L, 762561168L, 268852995L});
    std::cout << "random state before initialization\n";
    for (auto &i : engine.put()) {std::cout << i << " ";} std::cout << " -//- " <<  engine.getSeed() << "\n";

    hadronizer_.randomizeIndex(lumi,randomEngineSentry.randomEngine());
    hadronizer_.generateLHE(lumi,randomEngineSentry.randomEngine(), nThreads_);
    
    if ( !hadronizer_.readSettings(0) )
       throw edm::Exception(errors::Configuration) 
	 << "Failed to read settings for the hadronizer "
	 << hadronizer_.classname() << " \n";
        
    if ( decayer_ )
    {
       decayer_->init(es);
       if ( !hadronizer_.declareStableParticles( decayer_->operatesOnParticles() ) )
          throw edm::Exception(errors::Configuration)
            << "Failed to declare stable particles in hadronizer "
            << hadronizer_.classname()
            << "\n";
       if ( !hadronizer_.declareSpecialSettings( decayer_->specialSettings() ) )
          throw edm::Exception(errors::Configuration)
            << "Failed to declare special settings in hadronizer "
            << hadronizer_.classname()
            << "\n";
    }

    if ( !hadronizer_.initializeForInternalPartons() )
       throw edm::Exception(errors::Configuration) 
	 << "Failed to initialize hadronizer "
	 << hadronizer_.classname()
	 << " for internal parton generation\n";
         
    std::unique_ptr<GenLumiInfoHeader> genLumiInfoHeader(hadronizer_.getGenLumiInfoHeader());
    lumi.put(std::move(genLumiInfoHeader));
    initialized_ = true;
  }

  template <class HAD, class DEC>
  void
  GeneratorFilter<HAD, DEC>::endLuminosityBlock(LuminosityBlock const&, EventSetup const&)
  {
    hadronizer_.cleanLHE();
  }

  template <class HAD, class DEC>
  void
  GeneratorFilter<HAD,DEC>::endLuminosityBlockProduce(LuminosityBlock & lumi, EventSetup const&)
  {
    hadronizer_.statistics();    
    if ( decayer_ ) decayer_->statistics();

    GenRunInfoProduct genRunInfo = GenRunInfoProduct(hadronizer_.getGenRunInfo());
    std::vector<GenLumiInfoProduct::ProcessInfo> GenLumiProcess;
    const GenRunInfoProduct::XSec& xsec = genRunInfo.internalXSec();
    GenLumiInfoProduct::ProcessInfo temp;      
    temp.setProcess(0);
    temp.setLheXSec(xsec.value(), xsec.error()); // Pythia gives error of -1
    temp.setNPassPos(nEventsInLumiBlock_);
    temp.setNPassNeg(0);
    temp.setNTotalPos(nEventsInLumiBlock_);
    temp.setNTotalNeg(0);
    temp.setTried(nEventsInLumiBlock_, nEventsInLumiBlock_, nEventsInLumiBlock_);
    temp.setSelected(nEventsInLumiBlock_, nEventsInLumiBlock_, nEventsInLumiBlock_);
    temp.setKilled(nEventsInLumiBlock_, nEventsInLumiBlock_, nEventsInLumiBlock_);
    temp.setAccepted(0,-1,-1);
    temp.setAcceptedBr(0,-1,-1);
    GenLumiProcess.push_back(temp);

    std::unique_ptr<GenLumiInfoProduct> genLumiInfo(new GenLumiInfoProduct());
    genLumiInfo->setHEPIDWTUP(-1);
    genLumiInfo->setProcessInfo( GenLumiProcess );
        
    lumi.put(std::move(genLumiInfo));

    nEventsInLumiBlock_ = 0;

  }
}

#endif // gen_GeneratorFilter_h
