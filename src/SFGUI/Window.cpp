#include <SFGUI/Window.hpp>
#include <SFGUI/Context.hpp>
#include <SFGUI/RenderQueue.hpp>
#include <SFGUI/Engine.hpp>

#include <limits>

namespace sfg {

// Signals.
Signal::SignalID Window::OnCloseButton = 0;

Window::Window( char style ) :
	m_style( style ),
	m_dragging( false ),
	m_resizing( false )
{
}

Window::Ptr Window::Create( char style ) {
	Window::Ptr window( new Window( style ) );

	window->RequestResize();

	return window;
}

std::unique_ptr<RenderQueue> Window::InvalidateImpl() const {
	if( GetChild() ) {
		GetChild()->SetAllocation( GetClientRect() );
	}

	return Context::Get().GetEngine().CreateWindowDrawable( std::dynamic_pointer_cast<const Window>( shared_from_this() ) );
}

void Window::SetTitle( const sf::String& title ) {
	m_title = title;
	Invalidate();
}

const sf::String& Window::GetTitle() const {
	return m_title;
}

sf::FloatRect Window::GetClientRect() const {
	sf::FloatRect clientrect( { 0, 0 }, { GetAllocation().size.x, GetAllocation().size.y } );
	float border_width( Context::Get().GetEngine().GetProperty<float>( "BorderWidth", shared_from_this() ) );
	float gap( Context::Get().GetEngine().GetProperty<float>( "Gap", shared_from_this() ) );

	clientrect.position.x += border_width + gap;
	clientrect.position.y += border_width + gap;
	clientrect.size.x -= 2 * border_width + 2 * gap;
	clientrect.size.y -= 2 * border_width + 2 * gap;

	if( HasStyle( TITLEBAR ) ) {
		unsigned int title_font_size( Context::Get().GetEngine().GetProperty<unsigned int>( "FontSize", shared_from_this() ) );
		const sf::Font& title_font( *Context::Get().GetEngine().GetResourceManager().GetFont( Context::Get().GetEngine().GetProperty<std::string>( "FontName", shared_from_this() ) ) );
		float title_height(
			Context::Get().GetEngine().GetFontLineHeight( title_font, title_font_size ) +
			2 * Context::Get().GetEngine().GetProperty<float>( "TitlePadding", shared_from_this() )
		);

		clientrect.position.y += title_height;
		clientrect.size.y -= title_height;
	}

	return clientrect;
}

void Window::HandleSizeChange() {
	if( !GetChild() ) {
		return;
	}

	GetChild()->SetAllocation( GetClientRect() );
}

void Window::SetStyle( char style ) {
	m_style = style;

	// Make sure dragging and resizing operations are cancelled.
	m_dragging = false;
	m_resizing = false;

	RequestResize();
	Invalidate();

	if( GetChild() ) {
		GetChild()->SetAllocation( GetClientRect() );
	}
}

char Window::GetStyle() const {
	return m_style;
}

bool Window::HasStyle( Style style ) const {
	return (m_style & style) == style;
}

sf::Vector2f Window::CalculateRequisition() {
	float visual_border_width( Context::Get().GetEngine().GetProperty<float>( "BorderWidth", shared_from_this() ) );
	float gap( Context::Get().GetEngine().GetProperty<float>( "Gap", shared_from_this() ) );
	sf::Vector2f requisition( 2 * visual_border_width + 2 * gap, 2 * visual_border_width + 2 * gap );

	if( HasStyle( TITLEBAR ) ) {
		unsigned int title_font_size( Context::Get().GetEngine().GetProperty<unsigned int>( "FontSize", shared_from_this() ) );
		const sf::Font& title_font( *Context::Get().GetEngine().GetResourceManager().GetFont( Context::Get().GetEngine().GetProperty<std::string>( "FontName", shared_from_this() ) ) );
		float title_height(
			Context::Get().GetEngine().GetFontLineHeight( title_font, title_font_size ) +
			2 * Context::Get().GetEngine().GetProperty<float>( "TitlePadding", shared_from_this() )
		);

		requisition.y += title_height;
	}

	if( GetChild() ) {
		requisition += GetChild()->GetRequisition();
	}
	else {
		requisition.x = std::max( 50.f, requisition.x );
		requisition.y = std::max( 50.f, requisition.y * 2.f );
	}

	return requisition;
}

const std::string& Window::GetName() const {
	static const std::string name( "Window" );
	return name;
}

void Window::HandleMouseButtonEvent( sf::Mouse::Button button, bool press, int x, int y ) {
	if( button != sf::Mouse::Button::Left ) {
		return;
	}

	if( !press ) {
		m_dragging = false;
		m_resizing = false;
		return;
	}

	unsigned int title_font_size( Context::Get().GetEngine().GetProperty<unsigned int>( "FontSize", shared_from_this() ) );
	const sf::Font& title_font( *Context::Get().GetEngine().GetResourceManager().GetFont( Context::Get().GetEngine().GetProperty<std::string>( "FontName", shared_from_this() ) ) );
	float title_height(
		Context::Get().GetEngine().GetFontLineHeight( title_font, title_font_size ) +
		2 * Context::Get().GetEngine().GetProperty<float>( "TitlePadding", shared_from_this() )
	);

	// Check for mouse being inside the title area.
	sf::FloatRect area(
		GetAllocation().position,
		{ GetAllocation().size.x, title_height }
	);

	if( area.contains( sf::Vector2f( sf::Vector2( x, y ) ) ) ) {
		if( HasStyle( TITLEBAR ) && !m_dragging ) {
			if( HasStyle( CLOSE ) ) {
				auto close_height( Context::Get().GetEngine().GetProperty<float>( "CloseHeight", shared_from_this() ) );

				auto button_margin = ( title_height - close_height ) / 2.f;

				auto close_rect = sf::FloatRect(
					{ GetAllocation().position.x + GetAllocation().size.x - button_margin - close_height, GetAllocation().position.y + button_margin },
					{ close_height, close_height }
				);

				if( close_rect.contains( sf::Vector2f( sf::Vector2( x, y ) ) ) ) {
					GetSignals().Emit( OnCloseButton );
					return;
				}
			}

			m_dragging = true;
			m_resizing = false;

			m_drag_offset = sf::Vector2f(
				static_cast<float>( x ) - GetAllocation().position.x,
				static_cast<float>( y ) - GetAllocation().position.y
			);
		}
	}
	else {
		float handle_size( Context::Get().GetEngine().GetProperty<float>( "HandleSize", shared_from_this() ) );

		area.position.x = GetAllocation().position.x + GetAllocation().size.x - handle_size;
		area.position.y = GetAllocation().position.y + GetAllocation().size.y - handle_size;
		area.size.x = handle_size;
		area.size.y = handle_size;

		if( area.contains( sf::Vector2f( sf::Vector2( x, y ) ) ) ) {
			m_dragging = false;
			m_resizing = true;

			m_drag_offset = sf::Vector2f(
				handle_size - static_cast<float>( x ) + GetAllocation().position.x + GetAllocation().size.x - handle_size,
				handle_size - static_cast<float>( y ) + GetAllocation().position.y + GetAllocation().size.y - handle_size
			);
		}
	}

}

void Window::HandleMouseMoveEvent( int x, int y ) {
	if( ( x == std::numeric_limits<int>::min() ) || ( y == std::numeric_limits<int>::min() ) ) {
		return;
	}

	if( m_dragging ) {
		SetPosition(
			sf::Vector2f(
				static_cast<float>( x ) - m_drag_offset.x,
				static_cast<float>( y ) - m_drag_offset.y
			)
		);
	}
	else if( m_resizing && (GetStyle() & RESIZE) == RESIZE ) {
		SetAllocation(
			sf::FloatRect(
				GetAllocation().position,
				{ std::max( GetRequisition().x, static_cast<float>( x ) + m_drag_offset.x - GetAllocation().position.x ),
				  std::max( GetRequisition().y, static_cast<float>( y ) + m_drag_offset.y - GetAllocation().position.y ) }
			)
		);
	}
}

bool Window::HandleAdd( Widget::Ptr child ) {
	if( !Bin::HandleAdd( child ) ) {
		return false;
	}

	// Reset allocation so the window will be as large as required.
	SetAllocation( sf::FloatRect( GetAllocation().position, { 1.f, 1.f } ) );
	RequestResize();

	return true;
}

}
