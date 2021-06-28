SET( zyppng_media_SRCS
  media/device.cc
  media/devicehandler.cc
  media/devicemanager.cc
  media/medianetwork.cc
  media/medianetworkserver.cc
  media/mediasethandler.cc
  media/mediaverifier.cc
  media/pathref.cc
  media/network/downloader.cc
  media/network/downloadspec.cc
  media/network/mirrorcontrol.cc
  media/network/networkrequestdispatcher.cc
  media/network/networkrequesterror.cc
  media/network/request.cc
)

SET( zyppng_media_HEADERS
  media/AsyncDeviceOp
  media/asyncdeviceop.h
  media/Device
  media/device.h
  media/DeviceHandler
  media/devicehandler.h
  media/DeviceManager
  media/devicemanager.h
  media/MediaNetwork
  media/medianetwork.h
  media/mediasethandler.h
  media/MediaSetHandler
  media/mediaverifier.h
  media/MediaVerifier
  media/pathref.h
  media/PathRef
  media/network/AuthData
  media/network/authdata.h
  media/network/downloader.h
  media/network/downloadspec.h
  media/network/networkrequestdispatcher.h
  media/network/networkrequesterror.h
  media/network/request.h
  media/network/transfersettings.h
  media/network/TransferSettings
)

SET( zyppng_media_private_HEADERS
  media/handler/cdromhandler_p.h
  media/handler/networkhandler_p.h
  media/private/medianetworkserver_p.h
  media/private/pathref_p.h
  media/network/private/downloader_p.h
  media/network/private/mediadebug_p.h
  media/network/private/mirrorcontrol_p.h
  media/network/private/networkrequestdispatcher_p.h
  media/network/private/networkrequesterror_p.h
  media/network/private/request_p.h
  media/network/private/downloaderstates/base_p.h
  media/network/private/downloaderstates/basicdownloader_p.h
  media/network/private/downloaderstates/detectmeta_p.h
  media/network/private/downloaderstates/final_p.h
  media/network/private/downloaderstates/initial_p.h
  media/network/private/downloaderstates/metalinkinfo_p.h
  media/network/private/downloaderstates/metalink_p.h
  media/network/private/downloaderstates/mirrorhandling_p.h
  media/network/private/downloaderstates/normal_p.h
  media/network/private/downloaderstates/preparemulti_p.h
  media/network/private/downloaderstates/rangedownloader_p.h
  media/network/private/downloaderstates/zck_p.h
)

SET( zyppng_media_private_SOURCES
  media/handler/cdromhandler.cc
  media/handler/networkhandler.cc
  media/network/private/downloaderstates/basicdownloader_p.cc
  media/network/private/downloaderstates/detectmeta_p.cc
  media/network/private/downloaderstates/final_p.cc
  media/network/private/downloaderstates/initial_p.cc
  media/network/private/downloaderstates/metalinkinfo_p.cc
  media/network/private/downloaderstates/metalink_p.cc
  media/network/private/downloaderstates/mirrorhandling_p.cc
  media/network/private/downloaderstates/normal_p.cc
  media/network/private/downloaderstates/preparemulti_p.cc
  media/network/private/downloaderstates/rangedownloader_p.cc
  media/network/private/downloaderstates/zck_p.cc
)

SET( zyppng_appcode_HEADERS
  Context
  context.h
)

SET( zyppng_appcode_SRCS
  context.cc
)

SET ( zyppng_HEADERS
  ${zyppng_appcode_HEADERS}
  ${zyppng_media_HEADERS}
  ${zyppng_media_private_HEADERS}
)

SET ( zyppng_SRCS
  ${zyppng_appcode_SRCS}
  ${zyppng_media_SRCS}
  ${zyppng_media_private_SOURCES}
)

ADD_LIBRARY( zyppng-objlib OBJECT ${zyppng_SRCS} ${zyppng_HEADERS} )
#we include generated headers, so we need to wait for zypp-protobuf to be ready
add_dependencies( zyppng-objlib zypp-protobuf )
