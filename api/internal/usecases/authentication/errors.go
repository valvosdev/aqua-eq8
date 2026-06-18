package authentication

import "github.com/valvosdev/aqua-eq8/api/infrastructure/http"

var (
	ErrInvalidCredentials = &http.AppError{
		Code:    http.CodeUnauthorized,
		Message: "invalid credentials",
	}
	ErrUserDisabled = &http.AppError{
		Code:    http.CodeForbidden,
		Message: "user disabled",
	}
)
