/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
#include "private/booleanchoicerequest_p.h"
#include "private/userrequest_p.h"

#include <zypp-glib/private/globals_p.h>
#include <zypp-glib/ui/zyppenums-ui.h>

static void user_request_if_init( ZyppUserRequestInterface *iface );

ZYPP_DECLARE_GOBJECT_BOILERPLATE ( ZyppBooleanChoiceRequest, zypp_boolean_choice_request )

G_DEFINE_TYPE_WITH_CODE(ZyppBooleanChoiceRequest, zypp_boolean_choice_request, G_TYPE_OBJECT,
  G_IMPLEMENT_INTERFACE ( ZYPP_TYPE_USER_REQUEST, user_request_if_init )
  G_ADD_PRIVATE ( ZyppBooleanChoiceRequest )
)

ZYPP_DEFINE_GOBJECT_BOILERPLATE ( ZyppBooleanChoiceRequest, zypp_boolean_choice_request, ZYPP, BOOLEAN_CHOICE_REQUEST )

#define ZYPP_D() \
  ZYPP_GLIB_WRAPPER_D( ZyppBooleanChoiceRequest, zypp_boolean_choice_request )

// define the GObject stuff
enum {
  PROP_CPPOBJ  = 1,
  N_PROPERTIES
};

static GParamSpec *obj_properties[N_PROPERTIES] = { NULL, };

static void user_request_if_init( ZyppUserRequestInterface *iface )
{
  ZyppUserRequestImpl<ZyppBooleanChoiceRequest, zypp_boolean_choice_request_get_type, zypp_boolean_choice_request_get_private >::init_interface( iface );
}

static void zypp_boolean_choice_request_class_init (ZyppBooleanChoiceRequestClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  ZYPP_INIT_GOBJECT_BOILERPLATE_KLASS ( zypp_boolean_choice_request, gobject_class );
  obj_properties[PROP_CPPOBJ] = ZYPP_GLIB_ADD_CPPOBJ_PROP();
}

static void
zypp_boolean_choice_request_get_property (GObject    *object,
  guint       property_id,
  GValue     *value,
  GParamSpec *pspec)
{
  g_return_if_fail( ZYPP_IS_BOOLEAN_CHOICE_REQUEST (object) );
  switch (property_id)
  {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

static void
zypp_boolean_choice_request_set_property (GObject      *object,
  guint         property_id,
  const GValue *value,
  GParamSpec   *pspec)
{
  g_return_if_fail( ZYPP_IS_BOOLEAN_CHOICE_REQUEST (object) );
  ZyppBooleanChoiceRequest *self = ZYPP_BOOLEAN_CHOICE_REQUEST (object);

  ZYPP_D();

  switch (property_id)
  {
    case PROP_CPPOBJ: {
      g_return_if_fail( d->_constrProps ); // only if the constr props are still valid
      ZYPP_GLIB_SET_CPPOBJ_PROP( zyppng::BooleanChoiceRequestRef, value, d->_constrProps->_cppObj );
      break;
    }
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

zyppng::BooleanChoiceRequestRef &zypp_boolean_choice_request_get_cpp( ZyppBooleanChoiceRequest *self )
{
  ZYPP_D();
  return d->_cppObj;
}

const char * zypp_boolean_choice_request_get_label( ZyppBooleanChoiceRequest *self )
{
  ZYPP_D();
  return d->_cppObj->label().c_str();
}

void zypp_boolean_choice_request_set_choice ( ZyppBooleanChoiceRequest *self, gboolean choice )
{
  ZYPP_D();
  d->_cppObj->setChoice( static_cast<bool>(choice) );
}

gboolean zypp_boolean_choice_request_get_choice  ( ZyppBooleanChoiceRequest *self )
{
  ZYPP_D();
  return static_cast<gboolean>(d->_cppObj->choice());
}
