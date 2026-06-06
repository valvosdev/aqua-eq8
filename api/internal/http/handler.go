package http

import "github.com/labstack/echo/v5"

type HttpHandler interface {
	RegisterRoutes(g RouterGroups)
}

type RouterGroups struct {
	Public    *echo.Group
	Protected *echo.Group
}
