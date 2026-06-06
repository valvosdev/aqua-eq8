package usecases

import (
	"github.com/valvosdev/aqua-eq8/api/internal/usecases/authentication"
	"go.uber.org/fx"
)

// Module bundles all domain usecases together
var Module = fx.Module("usecases",

	fx.Provide(
		authentication.NewAuthenticationService,
	),
)
