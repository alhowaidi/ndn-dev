#include "producer.hpp"
#include "fstream"

namespace ndn {
namespace chunks {

Producer::Producer(const Name& prefix,
                   Face& face,
                   KeyChain& keyChain,
                   const security::SigningInfo& signingInfo,
                   time::milliseconds freshnessPeriod,
                   size_t maxSegmentSize,
                   bool isVerbose,
                   bool needToPrintVersion,
                   std::istream& is)
  : m_face(face)
  , m_keyChain(keyChain)
  , m_signingInfo(signingInfo)
  , m_freshnessPeriod(freshnessPeriod)
  , m_maxSegmentSize(maxSegmentSize)
  , m_isVerbose(isVerbose)
{
//  if (prefix.size() > 0 && prefix[-1].isVersion()) {
//    m_prefix = prefix.getPrefix(-1);
//    m_versionedPrefix = prefix;
//  }
//  else {
//    m_prefix = prefix;
//    m_versionedPrefix = Name(m_prefix).appendVersion();
//  }
//std::cerr << "m_prefix" << m_prefix << std::endl;
//std::cerr << "m_versionPrefix" << m_versionedPrefix << std::endl;
  //topulateStore(is);

  if (needToPrintVersion)
    std::cout << m_versionedPrefix[-1] << std::endl;

  m_face.setInterestFilter("/ndn/",
                           bind(&Producer::onInterest, this, _2),
                           RegisterPrefixSuccessCallback(),
                           bind(&Producer::onRegisterFailed, this, _1, _2));

  if (m_isVerbose)
    std::cerr << "Data published with name: " << m_versionedPrefix << std::endl;
}

void
Producer::run()
{
  m_face.processEvents();
}

void
Producer::onInterest(const Interest& interest)
{
  BOOST_ASSERT(m_store.size() > 0);
  
  if (m_isVerbose)
  {std::cerr << "Interest: " << interest << std::endl;
   std::cerr << "interest name: " << interest.getName() << std::endl;
  }
  const Name& name = interest.getName();
std::cerr << "Interest: " << interest << std::endl;
  std::cerr << "interest name: " << name << std::endl;
  const auto s=name[1];

  const std::string ss = interest.getName()[1].toUri();
//  std::cerr << "File name: " << ss << std::endl;
 // std::cerr << "store size: " << m_store.size() << std::endl;
  
  if (m_store.size() == 0) { // first time call
     //std::ifstream& is;
     std::cerr << "File name: " << ss << std::endl;
     m_prefix = name;
     m_versionedPrefix = Name(m_prefix).appendVersion();
     populateStore(ss);

  }
  
  shared_ptr<Data> data;
//std::cerr << "-----" << std::endl;
//std::cerr << "name.size()" << name.size() << std::endl;
//std::cerr << "m_versionedPre.size() " << m_versionedPrefix.size() << std::endl;
//std::cerr << "isPrefixOfName: " <<m_versionedPrefix.isPrefixOf(name) << std::endl;
//std::cerr << "isSegment: " << name[-1].isSegment() << std::endl; 

  //std::cerr << "is segment: " << name[-1].isSegment() << std::endl;
  // is this a discovery Interest or a sequence retrieval?
  if (name.size() == m_versionedPrefix.size() + 1 && m_versionedPrefix.isPrefixOf(name) &&
      name[-1].isSegment()) {
    const auto segmentNo = static_cast<size_t>(interest.getName()[-1].toSegment());
    // specific segment retrieval
    
    
    if (segmentNo < m_store.size()) {
      data = m_store[segmentNo];
      std::cerr << "sno: " << segmentNo << std::endl;
  //    std::cerr << "*data: " << *data << std::endl;
    }
//    else{
  //    std::cerr << "sno: " << segmentNo << std::endl;
    //}
  }
  else if (interest.matchesData(*m_store[0])) {
    // Interest has version and is looking for the first segment or has no version
    data = m_store[0];
    //std::cerr << "in else if"  << std::endl;
   // std::cerr << data << std::endl;
    //std::cerr << "mstore[0]: "<< *m_store[0] << std::endl;
  }
  else{
//    std::cerr << "in else!!" << std::endl;
  }

  if (data != nullptr) {
    if (m_isVerbose)
      std::cerr << "Data: " << *data << std::endl;

    m_face.put(*data);
  }
//else
//std::cerr << "nullptr " << std::endl;
}

void
Producer::populateStore(std::string fileName)
{
  BOOST_ASSERT(m_store.size() == 0);

  if (m_isVerbose)
    std::cerr << "Loading input ..." << std::endl;

  std::fstream is(fileName);

  //std::cerr << "buffer: "<<buffer.data() <<std::endl;
  std::vector<uint8_t> buffer(m_maxSegmentSize);
  while (is.good()) {
    is.read(reinterpret_cast<char*>(buffer.data()), buffer.size());
    const auto nCharsRead = is.gcount();
    if (nCharsRead > 0) {
      auto data = make_shared<Data>(Name(m_versionedPrefix).appendSegment(m_store.size()));
//      std::cerr << "data: " << data << std::endl;
      data->setFreshnessPeriod(m_freshnessPeriod);
      data->setContent(&buffer[0], nCharsRead);

      m_store.push_back(data);
    }
  }
//std::cerr << "done data " << std::endl;
  if (m_store.empty()) {
  //  std::cerr << "store is empty" << std::endl;
    auto data = make_shared<Data>(Name(m_versionedPrefix).appendSegment(0));
    data->setFreshnessPeriod(m_freshnessPeriod);
    m_store.push_back(data);
  }

  auto finalBlockId = name::Component::fromSegment(m_store.size() - 1);
  for (const auto& data : m_store) {
    data->setFinalBlockId(finalBlockId);
    m_keyChain.sign(*data, m_signingInfo);
  }

  if (m_isVerbose)
    std::cerr << "Created " << m_store.size() << " chunks for prefix " << m_prefix << std::endl;
}

void
Producer::onRegisterFailed(const Name& prefix, const std::string& reason)
{
  std::cerr << "ERROR: Failed to register prefix '"
            << prefix << "' (" << reason << ")" << std::endl;
  m_face.shutdown();
}

} // namespace chunks
} // namespace ndn
