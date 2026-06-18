package http

import (
	"net/http"

	"github.com/labstack/echo/v5"
)

func HTTPErrorHandler(c *echo.Context, err error) {

	appErr, ok := err.(*AppError)
	if !ok {
		c.JSON(
			http.StatusInternalServerError,
			map[string]string{
				"error": "internal server error",
			},
		)
		return
	}
	status := http.StatusInternalServerError
	switch appErr.Code {
	case CodeValidation:
		status = http.StatusBadRequest
	case CodeUnauthorized:
		status = http.StatusUnauthorized
	case CodeForbidden:
		status = http.StatusForbidden
	case CodeNotFound:
		status = http.StatusNotFound
	case CodeConflict:
		status = http.StatusConflict
	}
	c.JSON(status, map[string]string{
		"error": appErr.Message,
	})
}
