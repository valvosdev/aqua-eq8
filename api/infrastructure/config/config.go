package config

import (
	"fmt"
	"time"

	"github.com/caarlos0/env/v11"
)

type (
	Config struct {
		App     app
		HTTP    http
		Log     log
		JWT     jwt
		Db      db
		Metrics metrics
		Swagger swagger

		Migration migration
	}

	migration struct {
		SourceDir string `env:"AQUA_MIGRATION_SOURCE_DIR" envDefault:"../deployments/postgres/scripts"`
	}

	app struct {
		Name    string `env:"AQUA_APP_NAME" envDefault:"aqua-eq8-api"`
		Version string `env:"AQUA_APP_VERSION" envDefault:"v1.0.0"`
	}

	http struct {
		Port string `env:"AQUA_API_HTTP_PORT" envDefault:":4020"`
	}

	log struct {
		Level string `env:"AQUA_LOG_LEVEL" envDefault:"Debug"`
	}

	// JWT -.
	jwt struct {
		Secret      string        `env:"AQUA_JWT_SECRET,required"`
		TokenExpiry time.Duration `env:"AQUA_JWT_TOKEN_EXPIRY" envDefault:"24h"`
	}

	db struct {
		Host         string `env:"AQUA_DB_HOST" envDefault:"10.30.10.30"`
		Port         string `env:"AQUA_DB_PORT" envDefault:"5432"`
		User         string `env:"AQUA_DB_USERNAME,required"`
		Password     string `env:"AQUA_DB_PASSWORD,required"`
		DbName       string `env:"AQUA_DB_NAME,required"`
		SSLMode      string `env:"AQUA_DB_SSLMODE" envDefault:"disable"`
		MaxIdleConns int    `env:"AQUA_DB_MAX_IDLE_CONNS" envDefault:"10"`
		MaxOpenConns int    `env:"AQUA_DB_MAX_OPEN_CONNS" envDefault:"100"`
	}

	// Metrics -.
	metrics struct {
		Enabled bool `env:"AQUA_METRICS_ENABLED" envDefault:"false"`
	}

	// Swagger -.
	swagger struct {
		Enabled bool `env:"AQUA_SWAGGER_ENABLED" envDefault:"false"`
	}
)

func NewConfig() (*Config, error) {
	cfg := &Config{}
	if err := env.Parse(cfg); err != nil {
		return nil, fmt.Errorf("config error: %w", err)
	}

	return cfg, nil
}
