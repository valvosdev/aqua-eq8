package authentication

import (
	"context"

	"time"

	"github.com/valvosdev/aqua-eq8/api/internal/domain/user"
	"github.com/valvosdev/aqua-eq8/api/internal/storage/postgres"
	"golang.org/x/crypto/bcrypt"
)

type AuthService interface {
	Login(ctx context.Context, username, password string) (*LoginResponse, error)
}

type AuthServiceImpl struct {
	userRepository postgres.Repository[user.User]
}

func NewAuthenticationService(userRepo postgres.Repository[user.User]) AuthService {
	return &AuthServiceImpl{
		userRepository: userRepo,
	}
}

func (s *AuthServiceImpl) Login(ctx context.Context, username, password string) (*LoginResponse, error) {

	userEntity, err := s.userRepository.FindOne(ctx,
		map[string]any{
			"email": username,
		},
	)
	if err != nil {
		return nil, ErrInvalidCredentials
	}
	if !userEntity.IsActive() {
		return nil, ErrUserDisabled
	}
	err = bcrypt.CompareHashAndPassword(
		[]byte(userEntity.PasswordHash),
		[]byte(password),
	)
	if err != nil {
		return nil, ErrInvalidCredentials
	}
	now := time.Now().UTC()
	userEntity.LastLoginAt = &now
	if _, err := s.userRepository.Update(ctx, userEntity); err != nil {
		return nil, err
	}

	return &LoginResponse{
		UserID:   userEntity.ID,
		Email:    userEntity.Email,
		Fullname: userEntity.Fullname(),
		Roles:    []string{userEntity.Role},
		Status:   string(userEntity.Status),
		TenantID: userEntity.TenantID,
	}, nil
}
